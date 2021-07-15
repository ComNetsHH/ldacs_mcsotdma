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
			CPPUNIT_ASSERT(chosen_slot >= uint32_t(1) && chosen_slot <= link_manager->MIN_CANDIDATES);
		}

		void testScheduleBroadcastSlot() {
			link_manager->scheduleBroadcastSlot();
			CPPUNIT_ASSERT(link_manager->next_broadcast_slot >= uint32_t(1) && link_manager->next_broadcast_slot <= link_manager->MIN_CANDIDATES);
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

		void testContention() {
			auto *broadcast_packet = new L2Packet();
			broadcast_packet->addMessage(new L2HeaderBase(MacId(42), 0, 0, 0, 0), nullptr);
			broadcast_packet->addMessage(new L2HeaderBroadcast(), nullptr);

			// Zero broadcast rate so far.
			CPPUNIT_ASSERT_EQUAL(0.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());

			link_manager->onSlotStart(1);
			// Receive one packet.
			link_manager->onPacketReception(broadcast_packet);
			// 100% broadcasts so far
			CPPUNIT_ASSERT_EQUAL(1.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());
			link_manager->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(1.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());

			link_manager->onSlotStart(1);
			CPPUNIT_ASSERT_EQUAL(1.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());
			link_manager->onSlotEnd();
			// 50% broadcasts so far
			CPPUNIT_ASSERT_EQUAL(.5, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());

			link_manager->onSlotStart(1);
			CPPUNIT_ASSERT_EQUAL(.5, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());
			link_manager->onSlotEnd();
			// one broadcast in three slots so far
			CPPUNIT_ASSERT_EQUAL(1.0/3.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());

			link_manager->onSlotStart(1);
			CPPUNIT_ASSERT_EQUAL(1.0/3.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());
			link_manager->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(1.0/4.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());

			link_manager->onSlotStart(1);
			L2Packet *copy = broadcast_packet->copy();
			link_manager->onPacketReception(copy);
			CPPUNIT_ASSERT_EQUAL(2.0/5.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());
			link_manager->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(2.0/5.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());

			link_manager->onSlotStart(1);
			// two broadcast in five slots so far
			CPPUNIT_ASSERT_EQUAL(2.0/5.0, link_manager->contention_estimator.getAverageNonBeaconBroadcastRate());
		}

		void testCongestionWithBeacon() {
			auto *beacon_packet = new L2Packet();
			beacon_packet->addMessage(new L2HeaderBase(MacId(42), 0, 0, 0, 0), nullptr);
			beacon_packet->addMessage(new L2HeaderBeacon(), nullptr);

			// Zero broadcast rate so far.
			CPPUNIT_ASSERT_EQUAL(0.0, link_manager->congestion_estimator.getCongestion());
			// Receive one packet.
			link_manager->onSlotStart(1);
			link_manager->onPacketReception(beacon_packet);
			// 100% broadcasts so far
			CPPUNIT_ASSERT_EQUAL(1.0, link_manager->congestion_estimator.getCongestion());
			link_manager->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(1.0, link_manager->congestion_estimator.getCongestion());

			link_manager->onSlotStart(1);
			// 50% broadcasts so far
			CPPUNIT_ASSERT_EQUAL(1.0, link_manager->congestion_estimator.getCongestion());
			link_manager->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(.5, link_manager->congestion_estimator.getCongestion());

			link_manager->onSlotStart(1);
			link_manager->onSlotEnd();
			// one broadcast in three slots so far
			CPPUNIT_ASSERT_EQUAL(1.0/3.0, link_manager->congestion_estimator.getCongestion());

			link_manager->onSlotStart(1);
			beacon_packet = new L2Packet();
			beacon_packet->addMessage(new L2HeaderBase(MacId(42), 0, 0, 0, 0), nullptr);
			beacon_packet->addMessage(new L2HeaderBeacon(), nullptr);
			link_manager->onPacketReception(beacon_packet);
			// two broadcast in five slots so far
			CPPUNIT_ASSERT_EQUAL(2.0/4.0, link_manager->congestion_estimator.getCongestion());
			link_manager->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(2.0/4.0, link_manager->congestion_estimator.getCongestion());
			link_manager->onSlotStart(1);
			CPPUNIT_ASSERT_EQUAL(2.0/4.0, link_manager->congestion_estimator.getCongestion());
			link_manager->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(2.0/5.0, link_manager->congestion_estimator.getCongestion());
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

		void testScheduleNextBeacon() {
			size_t num_beacons_sent = 0;
			for (size_t t = 0; t < BeaconModule::MIN_BEACON_OFFSET*2.5; t++) {
				link_manager->onSlotStart(1);
				if (link_manager->beacon_module.shouldSendBeaconThisSlot()) {
					link_manager->onTransmissionBurstStart(0);
					num_beacons_sent++;
				}
				link_manager->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(size_t(2), num_beacons_sent);
		}

		void testParseBeacon() {
//			coutd.setVerbose(true);
			TestEnvironment env_you = TestEnvironment(partner_id, id);
			ReservationTable *table_1 = env_you.mac_layer->reservation_manager->getReservationTable(env_you.mac_layer->reservation_manager->getFreqChannelByCenterFreq(env_you.p2p_freq_1));
			ReservationTable *table_2 = env_you.mac_layer->reservation_manager->getReservationTable(env_you.mac_layer->reservation_manager->getFreqChannelByCenterFreq(env_you.p2p_freq_2));

			ReservationTable *table_1_me = env->mac_layer->reservation_manager->getReservationTable(env_you.mac_layer->reservation_manager->getFreqChannelByCenterFreq(env_you.p2p_freq_1));
			ReservationTable *table_2_me = env->mac_layer->reservation_manager->getReservationTable(env_you.mac_layer->reservation_manager->getFreqChannelByCenterFreq(env_you.p2p_freq_2));

			std::vector<int> slots_1 = {12, 23, 55}, slots_2 = {5, 6, 7};
			for (auto t : slots_1)
				table_1->mark(t, Reservation(MacId(100), Reservation::TX));
			for (auto t : slots_2)
				table_2->mark(t, Reservation(MacId(101), Reservation::TX));

			for (auto t : slots_1)
				CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table_1_me->getReservation(t));
			for (auto t : slots_2)
				CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table_2_me->getReservation(t));

			ReservationManager *manager = env_you.mac_layer->reservation_manager;
			auto beacon_msg = ((BCLinkManager*) env_you.mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->beacon_module.generateBeacon(manager->getP2PReservationTables(), manager->getBroadcastReservationTable());
			link_manager->processIncomingBeacon(partner_id, beacon_msg.first, beacon_msg.second);

			for (auto t : slots_1)
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::BUSY), table_1_me->getReservation(t));
			for (auto t : slots_2)
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::BUSY), table_2_me->getReservation(t));
		}

		/**
		 * If user1 has scheduled a beacon transmission during a slot that is utilized by another user, as it learns by parsing that user's beacon, it should re-schedule its own beacon transmission.
		 */
		void testParseBeaconRescheduleBeacon() {
			TestEnvironment env_you = TestEnvironment(partner_id, id);
			ReservationTable *bc_table_you = env_you.mac_layer->reservation_manager->getBroadcastReservationTable();
			int t = 5;
			bc_table_you->mark(t, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX));
			auto pair = ((BCLinkManager*) env_you.mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->beacon_module.generateBeacon({}, bc_table_you);

			auto* bc_lm = (BCLinkManager*) env->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			bc_lm->beacon_module.next_beacon_in = t;
			bc_lm->current_reservation_table->mark(t, Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON));
			CPPUNIT_ASSERT(bc_lm->beacon_module.next_beacon_in == t);
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON), bc_lm->current_reservation_table->getReservation(t));
			bc_lm->processIncomingBeacon(partner_id, pair.first, pair.second);
			CPPUNIT_ASSERT(bc_lm->beacon_module.next_beacon_in > t);
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), bc_lm->current_reservation_table->getReservation(t));
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON), bc_lm->current_reservation_table->getReservation(bc_lm->beacon_module.next_beacon_in));
		}

	CPPUNIT_TEST_SUITE(BCLinkManagerTests);
		CPPUNIT_TEST(testBroadcastSlotSelection);
		CPPUNIT_TEST(testScheduleBroadcastSlot);
		CPPUNIT_TEST(testBroadcast);
		CPPUNIT_TEST(testSendLinkRequestOnBC);
		CPPUNIT_TEST(testContention);
		CPPUNIT_TEST(testCongestionWithBeacon);
		CPPUNIT_TEST(testScheduleNextBeacon);
		CPPUNIT_TEST(testParseBeacon);
		CPPUNIT_TEST(testParseBeaconRescheduleBeacon);

//			CPPUNIT_TEST(testSetBeaconHeader);
//			CPPUNIT_TEST(testProcessIncomingBeacon);
//			CPPUNIT_TEST(testGetNumCandidateSlots);
//			CPPUNIT_TEST(testNotifyOutgoingSingle);
//			CPPUNIT_TEST(testNotifyOutgoingMulti);
		CPPUNIT_TEST_SUITE_END();
	};

}