//
// Created by Sebastian Lindner on 4/23/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"
#include "../P2PLinkManager.hpp"
#include "../BCLinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class ThreeUsersTests : public CppUnit::TestFixture {
	private:
		TestEnvironment* env1, * env2, * env3;
		MacId id1, id2, id3;
		uint64_t center_frequency1, center_frequency2, center_frequency3, bc_frequency, bandwidth;
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
			bc_frequency = env1->bc_frequency;
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
			MACLayer* mac_tx = env1->mac_layer, *mac_rx = env2->mac_layer, *mac_3 = env3->mac_layer;
			auto* p2p_tx = (P2PLinkManager*) mac_tx->getLinkManager(id2), *p2p_rx = (P2PLinkManager*) mac_rx->getLinkManager(id1);
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
			while (!env2->rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).empty() && num_slots++ < max_num_slots) {
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
			FrequencyChannel channel = FrequencyChannel(*p2p_tx->current_channel);
			ReservationTable *table_tx = p2p_tx->current_reservation_table,
				*table_rx = p2p_rx->current_reservation_table,
				*table_3 = env3->mac_layer->getReservationManager()->getReservationTable(&channel);
			coutd << "f=" << *table_tx->getLinkedChannel() << " f=" << *table_rx->getLinkedChannel() << " f=" << *table_3->getLinkedChannel() << std::endl;
			for (int t = 0; t < p2p_tx->getExpiryOffset() + p2p_tx->burst_offset*2; t++) {
				const Reservation &res_tx = table_tx->getReservation(t),
					&res_rx = table_rx->getReservation(t),
					&res_3 = table_3->getReservation(t);
				coutd << "t=" << t << ": " << res_tx << " | " << res_rx << " | " << res_3 << std::endl;
				if (res_tx.isIdle()) {
					CPPUNIT_ASSERT_EQUAL(res_tx, res_rx);
					CPPUNIT_ASSERT_EQUAL(res_tx, res_3);
				} else if (res_tx.isTx()) {
					CPPUNIT_ASSERT_EQUAL(Reservation(id1, Reservation::RX), res_rx);
					CPPUNIT_ASSERT_EQUAL(Reservation(id1, Reservation::BUSY), res_3);
				} else if (res_tx.isRx()) {
					CPPUNIT_ASSERT_EQUAL(Reservation(id1, Reservation::TX), res_rx);
					CPPUNIT_ASSERT_EQUAL(Reservation(id2, Reservation::BUSY), res_3);
				}
			}
		}

		void testLinkEstablishmentTwoUsersMultiSlot() {
//			coutd.setVerbose(true);
			unsigned long bits_per_slot = env1->phy_layer->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			num_outgoing_bits = expected_num_slots * bits_per_slot;
			((P2PLinkManager*) env1->mac_layer->getLinkManager(id2))->reported_desired_tx_slots = 1;
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
			auto* p2p_1 = (P2PLinkManager*) mac_1->getLinkManager(id2), *p2p_2 = (P2PLinkManager*) mac_2->getLinkManager(id3), *p2p_3 = (P2PLinkManager*) mac_3->getLinkManager(id2);

			// Trigger establishment.
			p2p_1->notifyOutgoing(num_outgoing_bits);
			p2p_2->notifyOutgoing(num_outgoing_bits);

			size_t num_slots = 0, max_num_slots = 100;
			while ((p2p_1->link_status != LinkManager::link_established || p2p_2->link_status != LinkManager::link_established || p2p_3->link_status != LinkManager::link_established) && num_slots++ < max_num_slots) {
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
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_1->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_2->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_3->link_status);

			num_slots = 0;
			size_t expected_num_packets = 5;
			while ((p2p_1->statistic_num_sent_packets < expected_num_packets || p2p_2->statistic_num_sent_packets < expected_num_packets) && num_slots++ < max_num_slots) {
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
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_1->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_2->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_3->link_status);
			CPPUNIT_ASSERT(p2p_1->statistic_num_sent_packets >= expected_num_packets);
			CPPUNIT_ASSERT(p2p_1->statistic_num_sent_packets > p2p_1->statistic_num_sent_requests);
			CPPUNIT_ASSERT(p2p_2->statistic_num_sent_packets >= expected_num_packets);
			CPPUNIT_ASSERT(p2p_2->statistic_num_sent_packets > p2p_2->statistic_num_sent_requests);
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
			((BCLinkManager*) env1->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->MIN_CANDIDATES = 3;
			((BCLinkManager*) env2->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->MIN_CANDIDATES = 3;
			auto* p2p_1_tx = (P2PLinkManager*) mac_1->getLinkManager(id2), *p2p_1_rx = (P2PLinkManager*) mac_2->getLinkManager(id1),
				  *p2p_2_tx = (P2PLinkManager*) mac_2->getLinkManager(id3), *p2p_2_rx = (P2PLinkManager*) mac_3->getLinkManager(id2);

			// Trigger establishment.
			p2p_1_tx->notifyOutgoing(num_outgoing_bits);
			p2p_2_tx->notifyOutgoing(num_outgoing_bits);

			size_t num_slots = 0, max_num_slots = 5000, num_renewals = 10;
			while ((p2p_1_tx->statistic_num_links_established < num_renewals || p2p_2_tx->statistic_num_links_established < num_renewals) && num_slots++ < max_num_slots) {
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
				CPPUNIT_ASSERT(!(p2p_1_tx->link_status == LinkManager::link_established && p2p_1_rx->link_status == LinkManager::link_not_established));
				CPPUNIT_ASSERT(!(p2p_2_tx->link_status == LinkManager::link_established && p2p_2_rx->link_status == LinkManager::link_not_established));
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
//			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_1_tx->link_status);
//			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_2_tx->link_status);
//			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_2_rx->link_status);

//			size_t expected_num_packets = p2p_1_tx->current_link_state->timeout;
//			size_t t_max = p2p_1_tx->current_link_state->timeout * p2p_1_tx->burst_offset + p2p_1_tx->burst_offset;
//			for (size_t t = 0; t < t_max; t++) {
//				mac_1->update(1);
//				mac_2->update(1);
//				mac_3->update(1);
//				mac_1->execute();
//				mac_2->execute();
//				mac_3->execute();
//				mac_1->onSlotEnd();
//				mac_2->onSlotEnd();
//				mac_3->onSlotEnd();
//			}
//
//			CPPUNIT_ASSERT(p2p_1_tx->link_status != LinkManager::link_not_established);
//			CPPUNIT_ASSERT(p2p_2_tx->link_status != LinkManager::link_not_established);
//			CPPUNIT_ASSERT(p2p_1_tx->statistic_num_sent_packets >= expected_num_packets);
//			CPPUNIT_ASSERT(p2p_1_tx->statistic_num_sent_packets > p2p_1_tx->statistic_num_sent_requests);
//			CPPUNIT_ASSERT(p2p_2_tx->statistic_num_sent_packets >= expected_num_packets);
//			CPPUNIT_ASSERT(p2p_2_tx->statistic_num_sent_packets > p2p_2_tx->statistic_num_sent_requests);
		}

		/**
		 * Tests that three users can communicate like so: A->B B->C.
		 * They initiate communication at exactly the same moment in time.
		 */
		void threeUsersNonOverlappingTest() {
//			coutd.setVerbose(true);
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer;
			env1->rlc_layer->should_there_be_more_p2p_data = false;
			env2->rlc_layer->should_there_be_more_p2p_data = false;
			env3->rlc_layer->should_there_be_more_p2p_data = false;
			env1->rlc_layer->should_there_be_more_p2p_data_map[id2] = true;
			env2->rlc_layer->should_there_be_more_p2p_data_map[id1] = false;
			env2->rlc_layer->should_there_be_more_p2p_data_map[id3] = true;
			env3->rlc_layer->should_there_be_more_p2p_data_map[id2] = false;
			auto* p2p_1 = (P2PLinkManager*) mac_1->getLinkManager(id2), *p2p_2 = (P2PLinkManager*) mac_2->getLinkManager(id3), *p2p_3 = (P2PLinkManager*) mac_3->getLinkManager(id2);
			p2p_1->notifyOutgoing(num_outgoing_bits);
			p2p_2->notifyOutgoing(num_outgoing_bits);

			size_t num_slots = 0, max_num_slots = 5000;
			while ((p2p_1->link_status != LinkManager::link_established || p2p_2->link_status != LinkManager::link_established || p2p_3->link_status != LinkManager::link_established) && num_slots++ < max_num_slots) {
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
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_1->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_2->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, p2p_3->link_status);
			CPPUNIT_ASSERT_EQUAL(size_t(1), p2p_1->statistic_num_links_established);
			CPPUNIT_ASSERT_EQUAL(size_t(1), p2p_2->statistic_num_links_established);
			CPPUNIT_ASSERT_EQUAL(size_t(1), p2p_3->statistic_num_links_established);

			unsigned long packets_so_far_1 = p2p_1->statistic_num_sent_packets, packets_so_far_2 = p2p_2->statistic_num_sent_packets;
			num_slots = 0;
			while ((p2p_1->statistic_num_links_established < 2 || p2p_2->statistic_num_links_established < 2  || p2p_3->statistic_num_links_established < 2 ) && num_slots++ < max_num_slots) {
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
			CPPUNIT_ASSERT_EQUAL(size_t(2), p2p_1->statistic_num_links_established);
			CPPUNIT_ASSERT_EQUAL(size_t(2), p2p_2->statistic_num_links_established);
			CPPUNIT_ASSERT_EQUAL(size_t(2), p2p_3->statistic_num_links_established);
			CPPUNIT_ASSERT(p2p_1->statistic_num_sent_packets > packets_so_far_1);
			CPPUNIT_ASSERT(p2p_2->statistic_num_sent_packets > packets_so_far_2);
			unsigned int num_renewals = 10;
			for (unsigned int n = 3; n < num_renewals; n++) {
				num_slots = 0;
				while ((p2p_1->statistic_num_links_established < n || p2p_2->statistic_num_links_established < n  || p2p_3->statistic_num_links_established < n ) && num_slots++ < max_num_slots) {
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
				if (num_slots >= max_num_slots)
					std::cerr << "oh no" << std::endl;
				CPPUNIT_ASSERT(num_slots < max_num_slots);
				CPPUNIT_ASSERT(p2p_1->statistic_num_links_established >= n);
				CPPUNIT_ASSERT(p2p_2->statistic_num_links_established >= n);
				CPPUNIT_ASSERT(p2p_3->statistic_num_links_established >= n);
				CPPUNIT_ASSERT(p2p_1->statistic_num_sent_packets > packets_so_far_1);
				CPPUNIT_ASSERT(p2p_2->statistic_num_sent_packets > packets_so_far_2);
				packets_so_far_1 = p2p_1->statistic_num_sent_packets;
				packets_so_far_2 = p2p_2->statistic_num_sent_packets;
			}
		}


		CPPUNIT_TEST_SUITE(ThreeUsersTests);
			CPPUNIT_TEST(testLinkEstablishmentTwoUsers);
			CPPUNIT_TEST(testLinkEstablishmentTwoUsersMultiSlot);
//			CPPUNIT_TEST(threeUsersLinkEstablishmentSameStart);
//			CPPUNIT_TEST(threeUsersLinkReestablishmentSameStart);
//			CPPUNIT_TEST(threeUsersNonOverlappingTest);
		CPPUNIT_TEST_SUITE_END();
	};
}