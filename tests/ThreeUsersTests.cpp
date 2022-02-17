//
// Created by Sebastian Lindner on 4/23/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"
#include "../PPLinkManager.hpp"
#include "../SHLinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class ThreeUsersTests : public CppUnit::TestFixture {
	private:
		TestEnvironment* env1, * env2, * env3;
		MacId id1, id2, id3;
		uint64_t center_frequency1, center_frequency2, center_frequency3, sh_frequency, bandwidth;
		uint32_t planning_horizon;
		size_t num_outgoing_bits;

	public:
		void setUp() override {
			id1 = MacId(42);
			id2 = MacId(43);
			id3 = MacId(44);
			env1 = new TestEnvironment(id1, id2);
			env2 = new TestEnvironment(id2, id1);
			env3 = new TestEnvironment(id3, id1);

			center_frequency1 = env1->p2p_freq_1;
			center_frequency2 = env1->p2p_freq_2;
			center_frequency3 = env1->p2p_freq_3;
			sh_frequency = env1->sh_frequency;
			bandwidth = env1->bandwidth;
			planning_horizon = env1->planning_horizon;

			env1->phy_layer->connected_phys.push_back(env2->phy_layer);
			env1->phy_layer->connected_phys.push_back(env3->phy_layer);

			env2->phy_layer->connected_phys.push_back(env1->phy_layer);
			env2->phy_layer->connected_phys.push_back(env3->phy_layer);

			env3->phy_layer->connected_phys.push_back(env1->phy_layer);
			env3->phy_layer->connected_phys.push_back(env2->phy_layer);

			num_outgoing_bits = 512;
		}

		void tearDown() override {
			delete env1;
			delete env2;
			delete env3;
		}

		/**
		 * Ensures that when two users communicate, the third is eventually informed through a LinkInfo.
		 */
		void testLinkEstablishmentTwoUsers() {
//			coutd.setVerbose(true);
			MACLayer *mac_tx = env1->mac_layer, *mac_rx = env2->mac_layer, *mac_3 = env3->mac_layer;
			auto *p2p_tx = (PPLinkManager*) mac_tx->getLinkManager(id2), *p2p_rx = (PPLinkManager*) mac_rx->getLinkManager(id1);
			p2p_tx->notifyOutgoing(num_outgoing_bits);
			size_t num_slots = 0, max_num_slots = 100;
			while (p2p_rx->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_tx->update(1);
				mac_rx->update(1);
				mac_3->update(1);
				mac_tx->execute();
				mac_rx->execute();
				mac_3->execute();
				mac_tx->onSlotEnd();
				mac_rx->onSlotEnd();
				mac_3->onSlotEnd();
				p2p_tx->notifyOutgoing(num_outgoing_bits);
			}
			while (p2p_tx->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_tx->update(1);
				mac_rx->update(1);
				mac_3->update(1);
				mac_tx->execute();
				mac_rx->execute();
				mac_3->execute();
				mac_tx->onSlotEnd();
				mac_rx->onSlotEnd();
				mac_3->onSlotEnd();
				p2p_tx->notifyOutgoing(num_outgoing_bits);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(false, env1->rlc_layer->isThereMoreData(SYMBOLIC_LINK_ID_BROADCAST));
			CPPUNIT_ASSERT_EQUAL(p2p_tx->link_status, LinkManager::Status::link_established);
			CPPUNIT_ASSERT_EQUAL(p2p_rx->link_status, LinkManager::Status::link_established);
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_3->stat_num_third_party_requests_rcvd.get());			
			FrequencyChannel channel = FrequencyChannel(*p2p_tx->current_channel);
			ReservationTable *table_tx = p2p_tx->current_reservation_table,
				*table_rx = p2p_rx->current_reservation_table,
				*table_3 = env3->mac_layer->getReservationManager()->getReservationTable(&channel);
			coutd << "f=" << *table_tx->getLinkedChannel() << " f=" << *table_rx->getLinkedChannel() << " f=" << *table_3->getLinkedChannel() << std::endl;
			int until = p2p_tx->link_state.timeout*p2p_tx->link_state.burst_offset + p2p_tx->getBurstOffset()*2;
			for (int t = 0; t < until; t++) {
				const Reservation &res_tx = table_tx->getReservation(t),
					&res_rx = table_rx->getReservation(t),
					&res_3 = table_3->getReservation(t);
				coutd << "t=" << t << ": " << res_tx << " | " << res_rx << " | " << res_3 << std::endl;
				if (res_tx.isIdle()) {
					CPPUNIT_ASSERT_EQUAL(res_tx, res_rx);
					CPPUNIT_ASSERT_EQUAL(res_tx, res_3);
				} else if (res_tx.isTx()) {
					CPPUNIT_ASSERT_EQUAL(Reservation(id1, Reservation::RX), res_rx);
					CPPUNIT_ASSERT_EQUAL(true, res_3.isBusy());
				} else if (res_tx.isRx()) {
					CPPUNIT_ASSERT_EQUAL(Reservation(id1, Reservation::TX), res_rx);
					CPPUNIT_ASSERT_EQUAL(true, res_3.isBusy());
				}
			}
		}

		void testLinkEstablishmentTwoUsersMultiSlot() {
//			coutd.setVerbose(true);
			unsigned long bits_per_slot = env1->phy_layer->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			num_outgoing_bits = expected_num_slots * bits_per_slot;
			((PPLinkManager*) env1->mac_layer->getLinkManager(id2))->setForceBidirectionalLinks(true);
			// Now do the other tests.
			testLinkEstablishmentTwoUsers();
		}

		/**
		 * Tests that three users can communicate like so: A->B B->C.
		 * They initiate communication at exactly the same time. Tests that links are established.
		 */
		void threeUsersLinkEstablishmentSameStart() {
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer;
			env1->rlc_layer->should_there_be_more_p2p_data_map[id2] = true;
			env2->rlc_layer->should_there_be_more_p2p_data_map[id1] = false;
			env2->rlc_layer->should_there_be_more_p2p_data_map[id3] = true;
			env3->rlc_layer->should_there_be_more_p2p_data_map[id2] = false;
			auto* p2p_1 = (PPLinkManager*) mac_1->getLinkManager(id2), *p2p_2 = (PPLinkManager*) mac_2->getLinkManager(id3), *p2p_3 = (PPLinkManager*) mac_3->getLinkManager(id2);

			// Trigger establishment.
			p2p_1->notifyOutgoing(num_outgoing_bits);
			p2p_2->notifyOutgoing(num_outgoing_bits);

			size_t num_slots = 0, max_num_slots = 1000;
			while ((mac_1->stat_num_pp_links_established.get() < 1 || mac_2->stat_num_pp_links_established.get() < 1 || mac_3->stat_num_pp_links_established.get() < 1) && num_slots++ < max_num_slots) {			
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_1->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_2->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_3->stat_num_pp_links_established.get());
		}

		/**
		 * Tests that three users can communicate like so: A->B B->C.
		 * They initiate communication at exactly the same time. Tests that links are re-established after expiry.
		 */
		void threeUsersLinkReestablishmentSameStart() {
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer;
			env1->rlc_layer->should_there_be_more_p2p_data_map[id2] = true;
			env2->rlc_layer->should_there_be_more_p2p_data_map[id1] = false;
			env2->rlc_layer->should_there_be_more_p2p_data_map[id3] = true;
			env3->rlc_layer->should_there_be_more_p2p_data_map[id2] = false;
			((SHLinkManager*) env1->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->MIN_CANDIDATES = 3;
			((SHLinkManager*) env2->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->MIN_CANDIDATES = 3;
			auto* p2p_1_tx = (PPLinkManager*) mac_1->getLinkManager(id2), *p2p_1_rx = (PPLinkManager*) mac_2->getLinkManager(id1),
				  *p2p_2_tx = (PPLinkManager*) mac_2->getLinkManager(id3), *p2p_2_rx = (PPLinkManager*) mac_3->getLinkManager(id2);

			// Trigger establishment.
			p2p_1_tx->notifyOutgoing(num_outgoing_bits);
			p2p_2_tx->notifyOutgoing(num_outgoing_bits);

			size_t num_slots = 0, max_num_slots = 15000, num_renewals = 1;
			while (((size_t) mac_1->stat_num_pp_links_established.get() < num_renewals || (size_t) mac_2->stat_num_pp_links_established.get() < num_renewals) && num_slots++ < max_num_slots) {
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_GREATEREQUAL(num_renewals, (size_t) mac_1->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(num_renewals, (size_t) mac_2->stat_num_pp_links_established.get());						
		}

		/**
		 * Tests that three users can communicate like so: A->B B->C.
		 * They initiate communication at exactly the same moment in time.
		 */
		void threeUsersNonOverlappingTest() {			
//			coutd.setVerbose(true);
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer;
			((SHLinkManager*) mac_1->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->beacon_module.setEnabled(false);
			((SHLinkManager*) mac_2->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->beacon_module.setEnabled(false);
			((SHLinkManager*) mac_3->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->beacon_module.setEnabled(false);
			mac_1->reportNeighborActivity(id2);
			mac_1->reportNeighborActivity(id3);
			mac_2->reportNeighborActivity(id1);
			mac_2->reportNeighborActivity(id3);
			mac_3->reportNeighborActivity(id1);
			mac_3->reportNeighborActivity(id2);
			env1->rlc_layer->should_there_be_more_p2p_data = true;
			env2->rlc_layer->should_there_be_more_p2p_data = true;
			env3->rlc_layer->should_there_be_more_p2p_data = true;			
			auto* p2p_1 = (PPLinkManager*) mac_1->getLinkManager(id2), *p2p_2 = (PPLinkManager*) mac_2->getLinkManager(id3), *p2p_3 = (PPLinkManager*) mac_3->getLinkManager(id2);
			p2p_1->notifyOutgoing(num_outgoing_bits);
			p2p_2->notifyOutgoing(num_outgoing_bits);

			size_t num_slots = 0, max_num_slots = 5000;
			while ((mac_1->stat_num_pp_links_established.get() < 1 || mac_2->stat_num_pp_links_established.get() < 1 || mac_3->stat_num_pp_links_established.get() < 1) && num_slots++ < max_num_slots) {
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
			}			
			CPPUNIT_ASSERT(num_slots < max_num_slots);			
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_1->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_2->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_3->stat_num_pp_links_established.get());

			unsigned long packets_so_far_1 = (size_t) mac_1->stat_num_requests_sent.get(), packets_so_far_2 = (size_t) mac_2->stat_num_packets_sent.get();
			num_slots = 0;
			while ((mac_1->stat_num_pp_links_expired.get() < 1 || mac_2->stat_num_pp_links_expired.get() < 1 || mac_3->stat_num_pp_links_expired.get() < 1) && num_slots++ < max_num_slots) {
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);			
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_1->stat_num_pp_links_expired.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_2->stat_num_pp_links_expired.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_3->stat_num_pp_links_expired.get());

			num_slots = 0;
			while (((size_t) mac_1->stat_num_pp_links_established.get() < 2 || (size_t) mac_2->stat_num_pp_links_established.get() < 2  || (size_t) mac_3->stat_num_pp_links_established.get() < 2 ) && num_slots++ < max_num_slots) {
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(2), (size_t) mac_1->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(2), (size_t) mac_2->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(2), (size_t) mac_3->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT((size_t) mac_1->stat_num_packets_sent.get() > packets_so_far_1);
			CPPUNIT_ASSERT((size_t) mac_2->stat_num_packets_sent.get() > packets_so_far_2);
			unsigned int num_renewals = 7;
			for (unsigned int n = 3; n < num_renewals; n++) {
				num_slots = 0;
				while (((size_t) mac_1->stat_num_pp_links_established.get() < n || (size_t) mac_2->stat_num_pp_links_established.get() < n  || (size_t) mac_3->stat_num_pp_links_established.get() < n ) && num_slots++ < max_num_slots) {
					mac_1->update(1);
					mac_2->update(1);
					mac_3->update(1);
					mac_1->execute();
					mac_2->execute();
					mac_3->execute();
					mac_1->onSlotEnd();
					mac_2->onSlotEnd();
					mac_3->onSlotEnd();
				}				
				CPPUNIT_ASSERT(num_slots < max_num_slots);
				CPPUNIT_ASSERT_GREATEREQUAL(size_t(n), (size_t) mac_1->stat_num_pp_links_established.get());
				CPPUNIT_ASSERT_GREATEREQUAL(size_t(n), (size_t) mac_2->stat_num_pp_links_established.get());
				CPPUNIT_ASSERT_GREATEREQUAL(size_t(n), (size_t) mac_3->stat_num_pp_links_established.get());
				CPPUNIT_ASSERT_GREATEREQUAL(packets_so_far_1, (size_t) mac_1->stat_num_packets_sent.get());
				CPPUNIT_ASSERT_GREATEREQUAL(packets_so_far_2, (size_t) mac_2->stat_num_packets_sent.get());
				packets_so_far_1 = (size_t) mac_1->stat_num_packets_sent.get();
				packets_so_far_2 = (size_t) mac_2->stat_num_packets_sent.get();
			}
		}		

		void testStatPacketsSent() {
			env1->rlc_layer->should_there_be_more_broadcast_data = false;
			env2->rlc_layer->should_there_be_more_broadcast_data = false;
			env3->rlc_layer->should_there_be_more_broadcast_data = false;			
			env1->rlc_layer->always_return_broadcast_payload = true;
			env2->rlc_layer->always_return_broadcast_payload = true;
			env3->rlc_layer->always_return_broadcast_payload = true;						
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer;
			std::vector<MACLayer*> macs = {mac_1, mac_2, mac_3};
			for (auto *mac : macs) {
				mac->setContentionMethod(ContentionMethod::naive_random_access);
				mac->setAlwaysScheduleNextBroadcastSlot(false);
				mac->setBcSlotSelectionMinNumCandidateSlots(3);
				mac->setBcSlotSelectionMaxNumCandidateSlots(3);
			}	

			size_t num_slots = 0, max_slots = 1000;
			// send at least t beacons
			while (mac_1->stat_num_beacons_sent.get() < 3.0 && num_slots++ < max_slots) {
				mac_1->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				mac_2->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				mac_3->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(3), (size_t) mac_1->stat_num_beacons_sent.get());						
			size_t num_packets_sent_1 = (size_t) mac_1->stat_num_packets_sent.get(), num_packets_sent_2 = (size_t) mac_2->stat_num_packets_sent.get(), num_packets_sent_3 = (size_t) mac_3->stat_num_packets_sent.get();
			size_t num_broadcasts_sent_1 = (size_t) mac_1->stat_num_broadcasts_sent.get(), num_broadcasts_sent_2 = (size_t) mac_2->stat_num_broadcasts_sent.get(), num_broadcasts_sent_3 = (size_t) mac_3->stat_num_broadcasts_sent.get();
			size_t num_beacons_sent_1 = (size_t) mac_1->stat_num_beacons_sent.get(), num_beacons_sent_2 = (size_t) mac_2->stat_num_beacons_sent.get(), num_beacons_sent_3 = (size_t) mac_3->stat_num_beacons_sent.get();			
			// no unicast-type packets should've been sent			
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_1->stat_num_replies_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_1->stat_num_requests_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_1->stat_num_unicasts_sent.get());			
			// now, the number of sent packets should equal the sum of broadcasts and beacons			
			CPPUNIT_ASSERT_EQUAL(num_broadcasts_sent_1 + num_beacons_sent_1, num_packets_sent_1);
			CPPUNIT_ASSERT_EQUAL(num_broadcasts_sent_2 + num_beacons_sent_2, num_packets_sent_2);
			CPPUNIT_ASSERT_EQUAL(num_broadcasts_sent_3 + num_beacons_sent_3, num_packets_sent_3);
		}		

		void testCollisions() {			
			env1->rlc_layer->should_there_be_more_broadcast_data = false;
			env2->rlc_layer->should_there_be_more_broadcast_data = false;
			env3->rlc_layer->should_there_be_more_broadcast_data = false;			
			env1->rlc_layer->always_return_broadcast_payload = true;
			env2->rlc_layer->always_return_broadcast_payload = true;
			env3->rlc_layer->always_return_broadcast_payload = true;						
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer;
			std::vector<MACLayer*> macs = {mac_1, mac_2, mac_3};
			for (auto *mac : macs) {
				mac->setContentionMethod(ContentionMethod::naive_random_access);
				mac->setAlwaysScheduleNextBroadcastSlot(false);
				mac->setBcSlotSelectionMinNumCandidateSlots(3);
				mac->setBcSlotSelectionMaxNumCandidateSlots(3);
			}		
			size_t max_slots = 1000;
			for (size_t t = 0; t < max_slots; t++) {
				mac_1->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				mac_2->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				mac_3->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
			}						
			size_t num_packets_sent_to_1 = (size_t) mac_2->stat_num_packets_sent.get() + (size_t) mac_3->stat_num_packets_sent.get();
			size_t num_packets_rcvd = (size_t) mac_1->stat_num_packets_rcvd.get();
			size_t num_packets_missed = (size_t) env1->phy_layer->stat_num_packets_missed.get();
			size_t num_packet_collisions = (size_t) mac_1->stat_num_packet_collisions.get();
			CPPUNIT_ASSERT_GREATER(size_t(0), num_packet_collisions);
			CPPUNIT_ASSERT_GREATER(size_t(0), num_packets_missed);			
			CPPUNIT_ASSERT_EQUAL(num_packets_rcvd + num_packets_missed + num_packet_collisions, num_packets_sent_to_1);
		}

		void testLinkEstablishmentThreeUsers() {
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer;
			mac_1->reportNeighborActivity(id2);
			mac_1->reportNeighborActivity(id3);
			mac_2->reportNeighborActivity(id1);
			mac_2->reportNeighborActivity(id3);
			mac_3->reportNeighborActivity(id1);
			mac_3->reportNeighborActivity(id2);
			auto *p2p_1 = (PPLinkManager*) mac_1->getLinkManager(id2), *p2p_2 = (PPLinkManager*) mac_2->getLinkManager(id1), *p2p_3 = (PPLinkManager*) mac_3->getLinkManager(id2);
			p2p_1->notifyOutgoing(num_outgoing_bits);
			p2p_3->notifyOutgoing(num_outgoing_bits);
			size_t num_slots = 0, max_num_slots = 200;
			while ((mac_1->stat_num_pp_links_established.get() < 1 || mac_2->stat_num_pp_links_established.get() < 1 || mac_3->stat_num_pp_links_established.get() < 1) && num_slots++ < max_num_slots) {			
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
				p2p_1->notifyOutgoing(num_outgoing_bits);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_1->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_2->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_3->stat_num_pp_links_established.get());				
		}


		CPPUNIT_TEST_SUITE(ThreeUsersTests);
			CPPUNIT_TEST(testLinkEstablishmentTwoUsers);
			CPPUNIT_TEST(testLinkEstablishmentTwoUsersMultiSlot);
			CPPUNIT_TEST(threeUsersLinkEstablishmentSameStart);			
			CPPUNIT_TEST(testStatPacketsSent);			
			CPPUNIT_TEST(testCollisions);
			CPPUNIT_TEST(threeUsersLinkReestablishmentSameStart);
			CPPUNIT_TEST(threeUsersNonOverlappingTest);
			CPPUNIT_TEST(testLinkEstablishmentThreeUsers);
		CPPUNIT_TEST_SUITE_END();
	};
}