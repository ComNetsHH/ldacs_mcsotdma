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
#include <L2Packet.hpp>
#include "MockLayers.hpp"
#include "../SHLinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class SHLinkManagerTests : public CppUnit::TestFixture {
	private:
		SHLinkManager *link_manager;
		MacId id, partner_id;
		uint32_t planning_horizon;
		MCSOTDMA_Mac *mac;
		TestEnvironment *env;

	public:
		void setUp() override {
			id = MacId(42);
			partner_id = MacId(43);
			env = new TestEnvironment(id, partner_id);
			planning_horizon = env->planning_horizon;
			mac = (MCSOTDMA_Mac*) env->mac_layer;
			link_manager = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
		}

		void tearDown() override {
			delete env;
		}

		void testBroadcastSlotSelection() {
			// No active neighbors -> just take next slot.
			unsigned int chosen_slot = link_manager->broadcastSlotSelection(1);
			CPPUNIT_ASSERT(chosen_slot >= uint32_t(1) && chosen_slot <= link_manager->MIN_CANDIDATES);
		}

		void testScheduleBroadcastSlot() {
			link_manager->scheduleBroadcastSlot();
			CPPUNIT_ASSERT_GREATEREQUAL(uint32_t(1), link_manager->next_broadcast_slot);
		}

		void testBroadcast() {
			link_manager->notifyOutgoing(1);
			env->rlc_layer->should_there_be_more_broadcast_data = true;
			size_t num_slots = 0, max_num_slots = 100;
			while (mac->stat_num_broadcasts_sent.get() < 1 && num_slots++ < max_num_slots) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());
		}

		/** Tests that a PPLinkManager forwards a link request to the SHLinkManager, which schedules a slot and transmits it. */
// 		void testSendLinkRequestOnBC() {
// //			coutd.setVerbose(true);
// 			mac->notifyOutgoing(512, partner_id);
// 			size_t num_slots = 0, max_num_slots = 100;
// 			while (mac->stat_num_broadcasts_sent.get() < 1 && num_slots++ < max_num_slots) {
// 				mac->update(1);
// 				mac->execute();
// 				mac->onSlotEnd();
// 			}
// 			CPPUNIT_ASSERT(num_slots < max_num_slots);
// 			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());
// 			L2Packet *link_request = env->phy_layer->outgoing_packets.at(0);
// 			CPPUNIT_ASSERT(link_request->getRequestIndex() > -1);
// 		}													

		void testAutoScheduleBroadcastSlotIfTheresNoData() {			
			// don't generate new broadcast data.
			env->rlc_layer->should_there_be_more_broadcast_data = false;
			// notify about queued, outgoing data
			link_manager->notifyOutgoing(128);
			env->rlc_layer->num_remaining_broadcast_packets = 1;
			// which should've scheduled a slot
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			// now it should be sent whenever the slot is scheduled and schedule a next one
			for (size_t t = 0; t < 100; t++) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
				CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			}
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			// check that every sent packet carries some info about the next broadcast slot
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), env->phy_layer->outgoing_packets.size());			
			for (auto *broadcast_packet : env->phy_layer->outgoing_packets) {				
				for (const auto *header : broadcast_packet->getHeaders()) {
					if (header->frame_type == L2Header::broadcast) {												
						CPPUNIT_ASSERT_GREATER(uint(0), ((L2HeaderSH*) header)->slot_offset);
					}
				}				
			}
		}

		void testAutoScheduleBroadcastSlotIfTheresData() {			
			// don't generate new broadcast data.
			env->rlc_layer->should_there_be_more_broadcast_data = true;
			// notify about queued, outgoing data
			link_manager->notifyOutgoing(128);
			// which should've scheduled a slot
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			// now it should be sent whenever the slot is scheduled and schedule a next one
			for (size_t t = 0; t < 100; t++) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
				CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			}
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			// check that every sent packet carries some info about the next broadcast slot
			CPPUNIT_ASSERT( env->phy_layer->outgoing_packets.size() > 1);
			for (auto *broadcast_packet : env->phy_layer->outgoing_packets) {				
				for (const auto *header : broadcast_packet->getHeaders()) {
					if (header->frame_type == L2Header::broadcast) {						
						CPPUNIT_ASSERT_GREATER(uint(0), ((L2HeaderSH*) header)->slot_offset);
						CPPUNIT_ASSERT_LESS(uint(1000), ((L2HeaderSH*) header)->slot_offset);						
					}
				}				
			}
		}		

		/** Ensures that the average number of slots inbetween broadcast packet generation is measured correctly. */
		void testAverageBroadcastSlotGenerationMeasurement() {
			CPPUNIT_ASSERT_EQUAL(uint32_t(0), link_manager->getAvgNumSlotsInbetweenPacketGeneration());
			unsigned int sending_interval = 5;
			size_t max_t = 100;
			for (size_t t = 0; t < max_t; t++) {
				mac->update(1);
				if (t % sending_interval == 0)
					link_manager->notifyOutgoing(512);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(sending_interval, link_manager->getAvgNumSlotsInbetweenPacketGeneration());
		}

		/** Ensures that when slot advertisement is on, the next broadcast slot is scheduled and advertised if there's no more data to send. */
		void testSlotAdvertisementWhenAutoAdvertisementIsOn() {			
			env->rlc_layer->should_there_be_more_broadcast_data = false;
			env->rlc_layer->num_remaining_broadcast_packets = 1;
			CPPUNIT_ASSERT_EQUAL(false, link_manager->next_broadcast_scheduled);
			// notify about new data
			link_manager->notifyOutgoing(1);
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			// broadcast this data
			size_t max_t = link_manager->next_broadcast_slot;
			for (size_t t = 0; t < max_t; t++) {
				mac->update(1);
				mac->execute();
				if (t < max_t - 1)
					mac->onSlotEnd(); // only end the slot *before* the transmission, otherwise 'next_broadcast_slot' may have decremented to zero already
			}
			// no new broadcast slot should've been scheduled
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			CPPUNIT_ASSERT( link_manager->next_broadcast_slot > 0);
			// make sure the header flag has been set in the broadcasted data packet
			auto &outgoing_packets = env->phy_layer->outgoing_packets;
			CPPUNIT_ASSERT_EQUAL(size_t(1), outgoing_packets.size());
			auto &packet = outgoing_packets.at(0);
			CPPUNIT_ASSERT_EQUAL(size_t(1), packet->getHeaders().size());
			const auto *base_header = (L2HeaderSH*) packet->getHeaders().at(0);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::broadcast, base_header->frame_type);
			CPPUNIT_ASSERT(base_header->slot_offset > 0);
		}

		/** Ensures that when slot advertisement is on, the next broadcast slot is scheduled and advertised if there's more data to send. */
		void testSlotAdvertisementWhenAutoAdvertisementIsOnAndTheresMoreData() {			
			env->rlc_layer->should_there_be_more_broadcast_data = true;
			CPPUNIT_ASSERT_EQUAL(false, link_manager->next_broadcast_scheduled);
			// notify about new data
			link_manager->notifyOutgoing(1);
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			// broadcast this data
			size_t max_t = link_manager->next_broadcast_slot;
			for (size_t t = 0; t < max_t; t++) {
				mac->update(1);
				mac->execute();
				if (t < max_t - 1)
					mac->onSlotEnd(); // only end the slot *before* the transmission, otherwise 'next_broadcast_slot' may have decremented to zero already
			}
			// no new broadcast slot should've been scheduled
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			CPPUNIT_ASSERT_GREATER(uint(0), link_manager->next_broadcast_slot);
			// make sure the header flag has been set in the broadcasted data packet
			auto &outgoing_packets = env->phy_layer->outgoing_packets;
			CPPUNIT_ASSERT_EQUAL(size_t(1), outgoing_packets.size());
			auto &packet = outgoing_packets.at(0);
			CPPUNIT_ASSERT_EQUAL(size_t(1), packet->getHeaders().size());
			const auto *base_header = (L2HeaderSH*) packet->getHeaders().at(0);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::broadcast, base_header->frame_type);
			CPPUNIT_ASSERT_GREATER(uint(0), base_header->slot_offset);
		}

		void testMacDelay() {
			// give it some data to send
			env->rlc_layer->should_there_be_more_broadcast_data = true;
			link_manager->notifyOutgoing(512);						
			std::vector<double> delays;
			size_t num_slots = 0, max_num_slots = 1000, num_tx = 10;
			while (env->mac_layer->stat_num_broadcasts_sent.get() < num_tx && num_slots++ < max_num_slots) {
				mac->update(1);
				mac->execute();
				if (env->mac_layer->stat_broadcast_mac_delay.wasUpdated())
					delays.push_back(env->mac_layer->stat_broadcast_mac_delay.get());
				mac->onSlotEnd();				
			}			
			CPPUNIT_ASSERT_LESS(max_num_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(num_tx, delays.size());
			for (const double &d : delays) {
				CPPUNIT_ASSERT_GREATEREQUAL(1.0, d);				
			}
		}		

		void testSHChannelAccessDelay() {
			const double target_collision_prob = .626;
			// zero neighbors
			double n = 0;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			unsigned int k = link_manager->getNumCandidateSlots(target_collision_prob, link_manager->MIN_CANDIDATES, link_manager->MAX_CANDIDATES);			
			unsigned int expected_k = std::max((double) link_manager->MIN_CANDIDATES, 1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, k);						
			// one neighbor
			mac->reportNeighborActivity(MacId(1));
			n = 1;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			unsigned int new_k = link_manager->getNumCandidateSlots(target_collision_prob, link_manager->MIN_CANDIDATES, link_manager->MAX_CANDIDATES);			
			expected_k = std::max(link_manager->MIN_CANDIDATES, (uint) std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n))));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATEREQUAL(k, new_k);
			k = new_k;
			// two neighbors
			mac->reportNeighborActivity(MacId(2));
			n = 2;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			new_k = link_manager->getNumCandidateSlots(target_collision_prob, link_manager->MIN_CANDIDATES, link_manager->MAX_CANDIDATES);			
			expected_k = std::max(link_manager->MIN_CANDIDATES, (uint) std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n))));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATEREQUAL(k, new_k);
			k = new_k;
			// three neighbors
			mac->reportNeighborActivity(MacId(3));
			n = 3;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			new_k = link_manager->getNumCandidateSlots(target_collision_prob, link_manager->MIN_CANDIDATES, link_manager->MAX_CANDIDATES);			
			expected_k = std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATER(k, new_k);
			k = new_k;
			// four neighbors
			mac->reportNeighborActivity(MacId(4));
			n = 4;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			new_k = link_manager->getNumCandidateSlots(target_collision_prob, link_manager->MIN_CANDIDATES, link_manager->MAX_CANDIDATES);			
			expected_k = std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATER(k, new_k);
			k = new_k;
			// five neighbors
			mac->reportNeighborActivity(MacId(5));
			n = 5;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());	
			new_k = link_manager->getNumCandidateSlots(target_collision_prob, link_manager->MIN_CANDIDATES, link_manager->MAX_CANDIDATES);			
			expected_k = std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATER(k, new_k);
			k = new_k;
		}

		/** During simulations, a maximum no. of candidate slots was observed, which didn't make much sense. */
		void testNoCandidateSlotsForParticularValues() {
			size_t num_neighbors = 60;
			for (size_t i = 0; i < num_neighbors; i++)
				mac->reportNeighborActivity(MacId(i));
			CPPUNIT_ASSERT_EQUAL(num_neighbors, mac->getNeighborObserver().getNumActiveNeighbors());
			double target_collision_prob = 0.05;			
			CPPUNIT_ASSERT_GREATER(uint(1000), link_manager->getNumCandidateSlots(target_collision_prob, link_manager->MIN_CANDIDATES, link_manager->MAX_CANDIDATES));
		}		

		void testDutyCycleMacDelay() {
			env->rlc_layer->should_there_be_more_broadcast_data = true;
			link_manager->scheduleBroadcastSlot();
			unsigned int broadcast_slot = link_manager->getNextBroadcastSlot();
			for (unsigned int t = 0; t < broadcast_slot; t++) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_broadcasts_sent.get());
			CPPUNIT_ASSERT_EQUAL(broadcast_slot, (uint) mac->stat_broadcast_mac_delay.get());
		}

		/** Tests that an advertised broadcast transmission is marked as such. */
		void testMarkAdvertisedBroadcastSlot() {
			L2HeaderSH *header = new L2HeaderSH(partner_id);
			header->slot_offset = 5;
			// currently idle
			CPPUNIT_ASSERT_EQUAL(Reservation(), link_manager->current_reservation_table->getReservation(header->slot_offset));
			// receive header
			link_manager->processBroadcastMessage(partner_id, header);			
			// now marked as RX
			CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), link_manager->current_reservation_table->getReservation(header->slot_offset));
			delete header;
		}

		/** Tests that an advertised broadcast transmission that indicates a collision leads to the rescheduling of one's own broadcast. */
		void testRescheduleBroadcastUponCollision() {
			L2HeaderSH *header = new L2HeaderSH(partner_id);
			header->slot_offset = 5;
			// currently TX
			link_manager->next_broadcast_scheduled = true;
			link_manager->next_broadcast_slot = header->slot_offset;
			link_manager->current_reservation_table->mark(header->slot_offset, Reservation(id, Reservation::TX));			
			// receive header
			link_manager->processBroadcastMessage(partner_id, header);			
			// now marked as RX
			CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), link_manager->current_reservation_table->getReservation(header->slot_offset));
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			CPPUNIT_ASSERT_GREATER(header->slot_offset, link_manager->next_broadcast_slot);
			delete header;
		}

		void testRememberAdvertisedSlotOffset() {
			CPPUNIT_ASSERT_THROW(mac->getNeighborObserver().getNextExpectedBroadcastSlotOffset(partner_id), std::invalid_argument);
			L2HeaderSH *header = new L2HeaderSH(partner_id);
			header->slot_offset = 5;
			link_manager->processBroadcastMessage(partner_id, header);
			CPPUNIT_ASSERT_EQUAL(header->slot_offset, mac->getNeighborObserver().getNextExpectedBroadcastSlotOffset(partner_id));
		}

		void testForgetAdvertisedSlotOffset() {
			CPPUNIT_ASSERT_THROW(mac->getNeighborObserver().getNextExpectedBroadcastSlotOffset(partner_id), std::invalid_argument);
			L2HeaderSH *header = new L2HeaderSH(partner_id);
			header->slot_offset = 5;
			link_manager->processBroadcastMessage(partner_id, header);
			mac->reportNeighborActivity(partner_id);
			CPPUNIT_ASSERT_EQUAL(header->slot_offset, mac->getNeighborObserver().getNextExpectedBroadcastSlotOffset(partner_id));
			for (size_t t = 0; t < header->slot_offset + 1; t++) {
				mac->neighbor_observer.onSlotEnd();
				if (t < header->slot_offset)
					CPPUNIT_ASSERT_EQUAL((unsigned int) (header->slot_offset - (t+1)), mac->getNeighborObserver().getNextExpectedBroadcastSlotOffset(partner_id));				
			}
			CPPUNIT_ASSERT_THROW(mac->getNeighborObserver().getNextExpectedBroadcastSlotOffset(partner_id), std::invalid_argument);
		}		

		/** Make sure that broadcasts are automatically scheduled. */
		void testAutoStartBroadcasts() {
			size_t max_slots = 1000;
			for (size_t num_slots = 0; num_slots < max_slots; num_slots++) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
				CPPUNIT_ASSERT_EQUAL(true, link_manager->isNextBroadcastScheduled());
			}
			CPPUNIT_ASSERT_EQUAL(true, link_manager->isNextBroadcastScheduled());
		}				

		void testFixedPPPeriod() {			
			int forced_period = 27;
			// before setting
			CPPUNIT_ASSERT_LESS(forced_period, link_manager->getPPMinOffsetAndPeriod().second);
			// set
			mac->setForcePPPeriod(true, forced_period);
			// after setting
			CPPUNIT_ASSERT_EQUAL(forced_period, link_manager->getPPMinOffsetAndPeriod().second);
		}		


	CPPUNIT_TEST_SUITE(SHLinkManagerTests);
		CPPUNIT_TEST(testBroadcastSlotSelection);
		CPPUNIT_TEST(testScheduleBroadcastSlot);
		CPPUNIT_TEST(testBroadcast);
		// CPPUNIT_TEST(testSendLinkRequestOnBC);						
		CPPUNIT_TEST(testAutoScheduleBroadcastSlotIfTheresNoData);
		CPPUNIT_TEST(testAutoScheduleBroadcastSlotIfTheresData);
		CPPUNIT_TEST(testAverageBroadcastSlotGenerationMeasurement);		
		CPPUNIT_TEST(testSlotAdvertisementWhenAutoAdvertisementIsOn);
		CPPUNIT_TEST(testSlotAdvertisementWhenAutoAdvertisementIsOnAndTheresMoreData);
		CPPUNIT_TEST(testMacDelay);		
		CPPUNIT_TEST(testSHChannelAccessDelay);	
		CPPUNIT_TEST(testNoCandidateSlotsForParticularValues);			
		CPPUNIT_TEST(testDutyCycleMacDelay);
		CPPUNIT_TEST(testMarkAdvertisedBroadcastSlot);
		CPPUNIT_TEST(testRescheduleBroadcastUponCollision);
		CPPUNIT_TEST(testRememberAdvertisedSlotOffset);		
		CPPUNIT_TEST(testForgetAdvertisedSlotOffset);								
		CPPUNIT_TEST(testAutoStartBroadcasts);
		CPPUNIT_TEST(testFixedPPPeriod);		
		CPPUNIT_TEST_SUITE_END();
	};

}
