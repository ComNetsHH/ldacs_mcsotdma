//
// Created by Sebastian Lindner on 09.12.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <L2Packet.hpp>
#include "MockLayers.hpp"
#include "../BCLinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class BCLinkManagerTests : public CppUnit::TestFixture {
	private:
		BCLinkManager *link_manager;
		MacId id, partner_id;
		uint32_t planning_horizon;
		MACLayer* mac;
		TestEnvironment *env;

	public:
		void setUp() override {
			id = MacId(42);
			partner_id = MacId(43);
			env = new TestEnvironment(id, partner_id, true);
			planning_horizon = env->planning_horizon;
			mac = env->mac_layer;
			link_manager = (BCLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
		}

		void tearDown() override {
			delete env;
		}

		void testBroadcastSlotSelection() {
			// No active neighbors -> just take next slot.
			unsigned int chosen_slot = link_manager->broadcastSlotSelection();
			CPPUNIT_ASSERT_EQUAL(uint32_t(1), chosen_slot);
		}

		void testScheduleBroadcastSlot() {
			link_manager->scheduleBroadcastSlot();
			CPPUNIT_ASSERT_EQUAL(uint32_t(1), link_manager->next_broadcast_slot);
		}

		void testBroadcast() {
			link_manager->notifyOutgoing(512);
			env->rlc_layer->should_there_be_more_broadcast_data = false;
			size_t num_slots = 0, max_num_slots = 100;
			while (link_manager->next_broadcast_scheduled && num_slots++ < max_num_slots) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());
		}

		/** Tests that a P2PLinkManager forwards a link request to the BCLinkManager, which schedules a slot and transmits it. */
		void testSendLinkRequestOnBC() {
//			coutd.setVerbose(true);
			mac->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_num_slots = 100;
			while (link_manager->next_broadcast_scheduled && num_slots++ < max_num_slots) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());
			L2Packet *link_request = env->phy_layer->outgoing_packets.at(0);
			CPPUNIT_ASSERT(link_request->getRequestIndex() > -1);
		}

//		void testSetBeaconHeader() {
//			L2HeaderBeacon header = L2HeaderBeacon();
//			link_manager->setHeaderFields(&header);
//			unsigned int num_hops = net_layer->getNumHopsToGroundStation();
//			CPPUNIT_ASSERT_EQUAL(num_hops, header.num_hops_to_ground_station);
//		}
//
//		void testProcessIncomingBeacon() {
//			Reservation reservation = Reservation(own_id, Reservation::TX);
//			ReservationTable* tbl = reservation_manager->getReservationTableByIndex(0);
//			tbl->mark(3, reservation);
//			CPPUNIT_ASSERT(tbl->getReservation(3) == reservation);
//			// This beacon should encapsulate the just-made reservation.
//			L2Packet* beacon = link_manager->prepareBeacon();
//			// Let's undo the reservation.
//			tbl->mark(3, Reservation());
//			CPPUNIT_ASSERT(tbl->getReservation(3) != reservation);
//			// Now we receive the beacon.
//			auto* beacon_header = (L2HeaderBeacon*) beacon->getHeaders().at(1);
//			auto* beacon_payload = (BeaconPayload*) beacon->getPayloads().at(1);
//			link_manager->processIncomingBeacon(MacId(10), beacon_header, beacon_payload);
//			// So the reservation should be made again.
//			CPPUNIT_ASSERT(tbl->getReservation(3) == reservation);
//			delete beacon;
//		}
//
//		void testGetNumCandidateSlots() {
//			link_manager->contention_estimator.reportBroadcast(communication_partner_id);
//			link_manager->contention_estimator.update();
//			link_manager->contention_estimator.update();
//			// 50% broadcast rate of the only neighbor
//			CPPUNIT_ASSERT_EQUAL(.5, link_manager->contention_estimator.getAverageBroadcastRate());
//			// 50% target collision probability
//			double target_collision_prob = .5;
//			uint expected_num_slots = 2; // => Picking 1 slot out of 2 has a collision probability of 50% then.
//			CPPUNIT_ASSERT_EQUAL(expected_num_slots, link_manager->getNumCandidateSlots(target_collision_prob));
//			// 5% target collision probability
//			target_collision_prob = .05;
//			expected_num_slots = 11; // comparing to MATLAB implementation
//			CPPUNIT_ASSERT_EQUAL(expected_num_slots, link_manager->getNumCandidateSlots(target_collision_prob));
//			MacId other_id = MacId(communication_partner_id.getId() + 1);
//			link_manager->contention_estimator.reportBroadcast(communication_partner_id);
//			link_manager->contention_estimator.reportBroadcast(other_id);
//			link_manager->contention_estimator.update();
//			link_manager->contention_estimator.update();
//			// 50% broadcast rate of *two* neighbors
//			CPPUNIT_ASSERT_EQUAL(.5, link_manager->contention_estimator.getAverageBroadcastRate());
//			expected_num_slots = 21; // comparing to MATLAB implementation
//			CPPUNIT_ASSERT_EQUAL(expected_num_slots, link_manager->getNumCandidateSlots(target_collision_prob));
//		}
//
//		void testNotifyOutgoingSingle() {
////				coutd.setVerbose(true);
//			mac->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
//			Reservation reservation = mac->reservation_manager->getBroadcastReservationTable()->getReservation(link_manager->lme->getMinOffset());
//			CPPUNIT_ASSERT_EQUAL(true, reservation.isTx());
//			CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, reservation.getTarget());
//			// So that querying whether there's more data returns false -> no next broadcast
//			rlc_layer->should_there_be_more_p2p_data = false;
//			CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
//			size_t num_slots = 0, max_slots = 10;
//			while (((OldBCLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->broadcast_slot_scheduled && num_slots++ < max_slots) {
//				mac->update(1);
//				mac->execute();
//			}
//			CPPUNIT_ASSERT(num_slots < max_slots);
//			CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());
//			L2Packet* packet = phy_layer->outgoing_packets.at(0);
//			CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, packet->getDestination());
//			auto* base_header = (L2HeaderBase*) packet->getHeaders().at(0);
//			CPPUNIT_ASSERT_EQUAL(own_id, base_header->icao_src_id);
//			CPPUNIT_ASSERT_EQUAL(own_id, packet->getOrigin());
//			CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, packet->getDestination());
//			CPPUNIT_ASSERT_EQUAL(uint(1), base_header->length_next);
//			CPPUNIT_ASSERT_EQUAL(uint(0), base_header->offset);
//			CPPUNIT_ASSERT_EQUAL(uint(0), base_header->timeout);
////				coutd.setVerbose(false);
//		}
//
//		void testNotifyOutgoingMulti() {
////				coutd.setVerbose(true);
//			rlc_layer->should_there_be_more_broadcast_data = true;
//			mac->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
//			Reservation reservation = mac->reservation_manager->getBroadcastReservationTable()->getReservation(link_manager->lme->getMinOffset());
//			CPPUNIT_ASSERT_EQUAL(true, reservation.isTx());
//			CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, reservation.getTarget());
//			// So that a next broadcast must be scheduled.
//			rlc_layer->should_there_be_more_p2p_data = true;
//			CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
//			size_t num_slots = 0, max_slots = 10;
//			while (phy_layer->outgoing_packets.empty() && num_slots++ < max_slots) {
//				mac->update(1);
//				mac->execute();
//			}
//			CPPUNIT_ASSERT(num_slots < max_slots);
//			CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());
//			L2Packet* packet = phy_layer->outgoing_packets.at(0);
//			CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, packet->getDestination());
//			auto* base_header = (L2HeaderBase*) packet->getHeaders().at(0);
//			CPPUNIT_ASSERT_EQUAL(uint(1), base_header->length_next);
//			// A non-zero offset means we must've scheduled a next broadcast.
//			CPPUNIT_ASSERT(base_header->offset > 0);
//			CPPUNIT_ASSERT_EQUAL(uint(0), base_header->timeout);
////				coutd.setVerbose(false);
//		}

	CPPUNIT_TEST_SUITE(BCLinkManagerTests);
		CPPUNIT_TEST(testBroadcastSlotSelection);
		CPPUNIT_TEST(testScheduleBroadcastSlot);
		CPPUNIT_TEST(testBroadcast);
		CPPUNIT_TEST(testSendLinkRequestOnBC);
//			CPPUNIT_TEST(testSetBeaconHeader);
//			CPPUNIT_TEST(testProcessIncomingBeacon);
//			CPPUNIT_TEST(testGetNumCandidateSlots);
//			CPPUNIT_TEST(testNotifyOutgoingSingle);
//			CPPUNIT_TEST(testNotifyOutgoingMulti);
		CPPUNIT_TEST_SUITE_END();
	};

}