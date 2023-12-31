// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"
#include "../PPLinkManager.hpp"
#include "../SHLinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class ManyUsersTests : public CppUnit::TestFixture {
	private:
		TestEnvironment *env1, *env2, *env3, *env4, *env5;
		MacId id1, id2, id3, id4, id5;
		uint64_t center_frequency1, center_frequency2, center_frequency3, sh_frequency, bandwidth;
		uint32_t planning_horizon;
		size_t num_outgoing_bits;

	public:
		void setUp() override {
			id1 = MacId(42);
			id2 = MacId(43);
			id3 = MacId(44);
			id4 = MacId(45);
			id5 = MacId(46);
			env1 = new TestEnvironment(id1, id2);
			env2 = new TestEnvironment(id2, id1);
			env3 = new TestEnvironment(id3, id1);
			env4 = new TestEnvironment(id4, id1);
			env5 = new TestEnvironment(id5, id1);

			center_frequency1 = env1->p2p_freq_1;
			center_frequency2 = env1->p2p_freq_2;
			center_frequency3 = env1->p2p_freq_3;
			sh_frequency = env1->sh_frequency;
			bandwidth = env1->bandwidth;
			planning_horizon = env1->planning_horizon;

			env1->phy_layer->connected_phys.push_back(env2->phy_layer);
			env1->phy_layer->connected_phys.push_back(env3->phy_layer);
			env1->phy_layer->connected_phys.push_back(env4->phy_layer);
			env1->phy_layer->connected_phys.push_back(env5->phy_layer);

			env2->phy_layer->connected_phys.push_back(env1->phy_layer);
			env2->phy_layer->connected_phys.push_back(env3->phy_layer);
			env2->phy_layer->connected_phys.push_back(env4->phy_layer);
			env2->phy_layer->connected_phys.push_back(env5->phy_layer);

			env3->phy_layer->connected_phys.push_back(env1->phy_layer);
			env3->phy_layer->connected_phys.push_back(env2->phy_layer);
			env3->phy_layer->connected_phys.push_back(env4->phy_layer);
			env3->phy_layer->connected_phys.push_back(env5->phy_layer);

			env4->phy_layer->connected_phys.push_back(env1->phy_layer);
			env4->phy_layer->connected_phys.push_back(env2->phy_layer);
			env4->phy_layer->connected_phys.push_back(env3->phy_layer);
			env4->phy_layer->connected_phys.push_back(env5->phy_layer);

			env5->phy_layer->connected_phys.push_back(env1->phy_layer);
			env5->phy_layer->connected_phys.push_back(env2->phy_layer);
			env5->phy_layer->connected_phys.push_back(env3->phy_layer);
			env5->phy_layer->connected_phys.push_back(env4->phy_layer);

			num_outgoing_bits = 512;
		}

		void tearDown() override {
			delete env1;
			delete env2;
			delete env3;
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

			size_t num_slots = 0, max_num_slots = 20000;
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

			size_t num_slots = 0, max_num_slots = 50000, num_renewals = 1;
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
			mac_1->setMinNumSupportedPPLinks(4);
			mac_2->setMinNumSupportedPPLinks(4);
			mac_3->setMinNumSupportedPPLinks(4);
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

			size_t num_slots = 0, max_num_slots = 15000;
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
				mac->setBcSlotSelectionMinNumCandidateSlots(3);
				mac->setBcSlotSelectionMaxNumCandidateSlots(3);
			}	

			size_t num_slots = 0, max_slots = 1000;
			// send at least t beacons
			while (mac_1->stat_num_broadcasts_sent.get() < 3.0 && num_slots++ < max_slots) {
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
			CPPUNIT_ASSERT_EQUAL(size_t(3), (size_t) mac_1->stat_num_broadcasts_sent.get());						
			size_t num_packets_sent_1 = (size_t) mac_1->stat_num_packets_sent.get(), num_packets_sent_2 = (size_t) mac_2->stat_num_packets_sent.get(), num_packets_sent_3 = (size_t) mac_3->stat_num_packets_sent.get();
			size_t num_broadcasts_sent_1 = (size_t) mac_1->stat_num_broadcasts_sent.get(), num_broadcasts_sent_2 = (size_t) mac_2->stat_num_broadcasts_sent.get(), num_broadcasts_sent_3 = (size_t) mac_3->stat_num_broadcasts_sent.get();			
			// no unicast-type packets should've been sent			
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_1->stat_num_replies_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_1->stat_num_requests_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_1->stat_num_unicasts_sent.get());			
			// now, the number of sent packets should equal the sum of broadcasts and beacons			
			CPPUNIT_ASSERT_EQUAL(num_broadcasts_sent_1, num_packets_sent_1);
			CPPUNIT_ASSERT_EQUAL(num_broadcasts_sent_2, num_packets_sent_2);
			CPPUNIT_ASSERT_EQUAL(num_broadcasts_sent_3, num_packets_sent_3);
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
				mac->setBcSlotSelectionMinNumCandidateSlots(3);
				mac->setBcSlotSelectionMaxNumCandidateSlots(3);
			}		
			size_t max_slots = 3000;
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
			// CPPUNIT_ASSERT_GREATER(size_t(0), num_packets_missed);			
			CPPUNIT_ASSERT_EQUAL(num_packets_rcvd + num_packets_missed + (num_packet_collisions*2), num_packets_sent_to_1);
		}

		void testTwoRequestsForSameAdvertisedResource() {
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer;						
			size_t num_slots = 0, max_slots = 1000;			
			while ((mac_1->getNeighborObserver().getNumActiveNeighbors() < 2 || mac_2->getNeighborObserver().getNumActiveNeighbors() < 2 || mac_3->getNeighborObserver().getNumActiveNeighbors() < 2) && num_slots++ < max_slots) {
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
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_1->stat_num_broadcasts_sent.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_2->stat_num_broadcasts_rcvd.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_3->stat_num_broadcasts_rcvd.get());
			// have MAC2 and MAC3 establish links
			((SHLinkManager*) mac_1->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setShouldTransmit(false);
			((SHLinkManager*) mac_1->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->unscheduleBroadcastSlot();
			((SHLinkManager*) mac_2->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setShouldTransmit(true);
			((SHLinkManager*) mac_3->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setShouldTransmit(true);
			mac_2->notifyOutgoing(1, id1);
			mac_3->notifyOutgoing(1, id1);
			num_slots = 0;
			while (mac_1->stat_num_requests_rcvd.get() < 6 && num_slots++ < max_slots) {
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
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(6), (size_t) mac_1->stat_num_requests_rcvd.get());			
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
			size_t num_slots = 0, max_num_slots = 500;
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

		/** #134: during simulations, it was observed that when we have A-B-C, where A doesn't see C, then the link A-B is initiated in adequate time, but B-C is not. */
		void testHiddenNodeScenario() {
			env1->phy_layer->connected_phys.clear();
			env2->phy_layer->connected_phys.clear();
			env3->phy_layer->connected_phys.clear();			
			// A-B
			env1->phy_layer->connected_phys.push_back(env2->phy_layer);
			env2->phy_layer->connected_phys.push_back(env1->phy_layer);
			// B-C
			env2->phy_layer->connected_phys.push_back(env3->phy_layer);
			env3->phy_layer->connected_phys.push_back(env2->phy_layer);			
			// get pointers
			MACLayer *mac_A = env1->mac_layer, *mac_B = env2->mac_layer, *mac_C = env3->mac_layer;
			auto *p2p_A = (PPLinkManager*) mac_A->getLinkManager(id2), *p2p_B = (PPLinkManager*) mac_B->getLinkManager(id1), *p2p_C = (PPLinkManager*) mac_C->getLinkManager(id2);
			// trigger establishment
			p2p_A->notifyOutgoing(1);
			p2p_C->notifyOutgoing(1);
			size_t num_slots = 0, max_slots = 50000;
			size_t num_link_establishments = 2, links_A = 0, links_C = 0;
			double avg_link_estbl_time_A = 0, avg_link_estbl_time_C = 0;
			while ((mac_A->stat_num_pp_links_established.get() < num_link_establishments || mac_C->stat_num_pp_links_established.get() < num_link_establishments) && num_slots++ < max_slots) {
				mac_A->update(1);
				mac_B->update(1);
				mac_C->update(1);
				mac_A->execute();
				mac_B->execute();
				mac_C->execute();
				mac_A->onSlotEnd();
				mac_B->onSlotEnd();
				mac_C->onSlotEnd();
				if (mac_A->stat_num_pp_links_established.get() > links_A) {
					links_A = mac_A->stat_num_pp_links_established.get();
					avg_link_estbl_time_A += mac_A->stat_pp_link_establishment_time.get();
					// std::cout << "slot=" << num_slots << " link A-B: " << mac_A->stat_pp_link_establishment_time.get() << std::endl;
				}
				if (mac_C->stat_num_pp_links_established.get() > links_C) {
					links_C = mac_C->stat_num_pp_links_established.get();
					avg_link_estbl_time_C += mac_C->stat_pp_link_establishment_time.get();
					// std::cout << "slot=" << num_slots << " link C-B: " << mac_C->stat_pp_link_establishment_time.get() << std::endl;
				}
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_GREATEREQUAL(num_link_establishments, (size_t) mac_A->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(num_link_establishments, (size_t) mac_B->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(num_link_establishments, (size_t) mac_C->stat_num_pp_links_established.get());
			avg_link_estbl_time_A /= mac_A->stat_num_pp_links_established.get();
			avg_link_estbl_time_C /= mac_C->stat_num_pp_links_established.get();
			// link establishment time can vary a bit
			// I just test it to be "adequately small" with an arbitrary value...
			CPPUNIT_ASSERT_LESS(200.0, avg_link_estbl_time_A);
			CPPUNIT_ASSERT_LESS(200.0, avg_link_estbl_time_C);			
		}


		void testTwoLinksToOneUser() {
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer;
			// both 1 and 2 want to establish links with 3			
			auto *pp_1 = (PPLinkManager*) mac_1->getLinkManager(id3);
			auto *pp_2 = (PPLinkManager*) mac_2->getLinkManager(id3);
			pp_1->notifyOutgoing(1);
			pp_2->notifyOutgoing(1);
			size_t num_slots = 0, max_slots = 5000;
			while ((mac_1->stat_num_pp_links_established.get() < 1 || mac_2->stat_num_pp_links_established.get() < 1) && num_slots++ < max_slots) {
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
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_1->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_2->stat_num_pp_links_established.get());
		}

		void testEstablishFourLinks() {
			MACLayer *mac_1 = env1->mac_layer, *mac_2 = env2->mac_layer, *mac_3 = env3->mac_layer, *mac_4 = env4->mac_layer, *mac_5 = env5->mac_layer;
			mac_1->setMinNumSupportedPPLinks(4);
			mac_2->setMinNumSupportedPPLinks(4);
			mac_3->setMinNumSupportedPPLinks(4);
			mac_4->setMinNumSupportedPPLinks(4);
			mac_5->setMinNumSupportedPPLinks(4);
			size_t warmup = 500;
			for (size_t t = 0; t < warmup; t++) {
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_4->update(1);
				mac_5->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_4->execute();
				mac_5->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
				mac_4->onSlotEnd();
				mac_5->onSlotEnd();
			}
			mac_1->notifyOutgoing(1, id2);
			mac_1->notifyOutgoing(1, id3);
			mac_1->notifyOutgoing(1, id4);
			mac_1->notifyOutgoing(1, id5);
			size_t num_slots = 0, max_slots = 10000;
			while (mac_1->stat_num_pp_links_established.get() < 4.0 && num_slots++ < max_slots) {
				mac_1->update(1);
				mac_2->update(1);
				mac_3->update(1);
				mac_4->update(1);
				mac_5->update(1);
				mac_1->execute();
				mac_2->execute();
				mac_3->execute();
				mac_4->execute();
				mac_5->execute();
				mac_1->onSlotEnd();
				mac_2->onSlotEnd();
				mac_3->onSlotEnd();
				mac_4->onSlotEnd();
				mac_5->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			auto pp_budget = mac_1->getUsedPPDutyCycleBudget();
			size_t num_active_pp = pp_budget.first.size();
			CPPUNIT_ASSERT_EQUAL(size_t(4), num_active_pp);
		}


		CPPUNIT_TEST_SUITE(ManyUsersTests);			
			// CPPUNIT_TEST(threeUsersLinkEstablishmentSameStart);			
			// CPPUNIT_TEST(testStatPacketsSent);			
		// TODO	// CPPUNIT_TEST(testCollisions);
			// CPPUNIT_TEST(threeUsersLinkReestablishmentSameStart);
			CPPUNIT_TEST(threeUsersNonOverlappingTest);
			// CPPUNIT_TEST(testTwoRequestsForSameAdvertisedResource);
			// CPPUNIT_TEST(testLinkEstablishmentThreeUsers);
			// CPPUNIT_TEST(testHiddenNodeScenario);
			// CPPUNIT_TEST(testTwoLinksToOneUser);			
			CPPUNIT_TEST(testEstablishFourLinks);						
		CPPUNIT_TEST_SUITE_END();
	};
}