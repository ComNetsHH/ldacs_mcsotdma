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
		MACLayer *mac;
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
			unsigned int chosen_slot = link_manager->broadcastSlotSelection(1);
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
			while (mac->stat_num_broadcasts_sent.get() < 1 && num_slots++ < max_num_slots) {
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
			while (mac->stat_num_broadcasts_sent.get() < 1 && num_slots++ < max_num_slots) {
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
			// If it's enabled, it'll schedule its own initial beacon, messing up the hand-crafted tests.
			link_manager->beacon_module.setEnabled(false);
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
			link_manager->processBeaconMessage(partner_id, beacon_msg.first, beacon_msg.second);

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
			bc_lm->next_beacon_scheduled = true;
			CPPUNIT_ASSERT(bc_lm->beacon_module.next_beacon_in == t);
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON), bc_lm->current_reservation_table->getReservation(t));
			bc_lm->processBeaconMessage(partner_id, pair.first, pair.second);
			CPPUNIT_ASSERT(bc_lm->beacon_module.next_beacon_in > t);
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), bc_lm->current_reservation_table->getReservation(t));
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON), bc_lm->current_reservation_table->getReservation(bc_lm->beacon_module.next_beacon_in));
		}

		/**
		 * If user1 has scheduled a broadcast transmission during a slot that is utilized by another user, as it learns by parsing that user's beacon, it should re-schedule its own broadcast transmission.
		 */
		void testParseBeaconRescheduleBroadcast() {
			// schedule some broadcast slot
			auto* bc_lm = (BCLinkManager*) env->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			bc_lm->scheduleBroadcastSlot();
			// which turned out to be 't'
			int t = (int) bc_lm->next_broadcast_slot;
			CPPUNIT_ASSERT(t > 0);

			TestEnvironment env_you = TestEnvironment(partner_id, id);
			ReservationTable *bc_table_you = env_you.mac_layer->reservation_manager->getBroadcastReservationTable();
			// now have another user schedule its broadcast also at 't'
			bc_table_you->mark(t, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX));
			// which will be notified to the first user through a beacon
			auto pair = ((BCLinkManager*) env_you.mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->beacon_module.generateBeacon({}, bc_table_you);

			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX), bc_lm->current_reservation_table->getReservation(t));
			// which is processed
			bc_lm->processBeaconMessage(partner_id, pair.first, pair.second);
			// and now the first user should've moved away from 't'
			CPPUNIT_ASSERT(bc_lm->next_broadcast_slot != t);
			// and marked the slot as BUSY
			CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::BUSY), bc_lm->current_reservation_table->getReservation(t));
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX), bc_lm->current_reservation_table->getReservation(bc_lm->next_broadcast_slot));
		}

		void testBeaconDestination() {
			auto *packet = new L2Packet();
			auto *base_header = new L2HeaderBase(MacId(42), 0, 1, 1, 0);
			packet->addMessage(base_header, nullptr);
			packet->addMessage(link_manager->beacon_module.generateBeacon(link_manager->reservation_manager->getP2PReservationTables(), link_manager->reservation_manager->getBroadcastReservationTable()));
			CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BEACON, packet->getDestination());
		}

		void testDontScheduleNextBroadcastSlot() {
			// don't auto-schedule a next slot => only do so if there's more data.
			link_manager->setAlwaysScheduleNextBroadcastSlot(false);
			// don't generate new broadcast data.
			env->rlc_layer->should_there_be_more_broadcast_data = false;
			// notify about queued, outgoing data
			link_manager->notifyOutgoing(128);
			// which should've scheduled a slot
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			// now it should be sent whenever the slot is scheduled and *not* schedule a next one
			size_t num_slots = 0, max_slots = 100;
			while (link_manager->next_broadcast_scheduled && num_slots++ < max_slots) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(false, link_manager->next_broadcast_scheduled);
			// check that the single sent packet carries no info about the next broadcast slot
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());
			L2Packet *broadcast_packet = env->phy_layer->outgoing_packets.at(0);
			bool found_base_header = false;
			for (const auto *header : broadcast_packet->getHeaders()) {
				if (header->frame_type == L2Header::base) {
					found_base_header = true;
					auto *base_header = (L2HeaderBase*) header;
					CPPUNIT_ASSERT_EQUAL(uint32_t(0), base_header->burst_offset);
				}
			}
			CPPUNIT_ASSERT_EQUAL(true, found_base_header);
		}

		void testScheduleNextBroadcastSlotIfTheresData() {
			// don't auto-schedule a next slot => only do so if there's more data.
			link_manager->setAlwaysScheduleNextBroadcastSlot(false);
			// do generate new broadcast data.
			env->rlc_layer->should_there_be_more_broadcast_data = true;
			// notify about queued, outgoing data
			link_manager->notifyOutgoing(128);
			// which should've scheduled a slot
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_broadcast_scheduled);
			// now it should be sent whenever the slot is scheduled and *not* schedule a next one
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
				bool found_base_header = false;
				for (const auto *header : broadcast_packet->getHeaders()) {
					if (header->frame_type == L2Header::base) {
						found_base_header = true;
						auto *base_header = (L2HeaderBase*) header;
						CPPUNIT_ASSERT( base_header->burst_offset > 0);
					}
				}
				CPPUNIT_ASSERT_EQUAL(true, found_base_header);
			}
		}

		void testAutoScheduleBroadcastSlotIfTheresNoData() {
			// do auto-schedule a next slot => only do so if there's more data.
			link_manager->setAlwaysScheduleNextBroadcastSlot(true);
			// don't generate new broadcast data.
			env->rlc_layer->should_there_be_more_broadcast_data = false;
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
				bool found_base_header = false;
				for (const auto *header : broadcast_packet->getHeaders()) {
					if (header->frame_type == L2Header::base) {
						found_base_header = true;
						auto *base_header = (L2HeaderBase*) header;
						CPPUNIT_ASSERT( base_header->burst_offset > 0);
					}
				}
				CPPUNIT_ASSERT_EQUAL(true, found_base_header);
			}
		}

		void testAutoScheduleBroadcastSlotIfTheresData() {
			// do auto-schedule a next slot => only do so if there's more data.
			link_manager->setAlwaysScheduleNextBroadcastSlot(true);
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
				bool found_base_header = false;
				for (const auto *header : broadcast_packet->getHeaders()) {
					if (header->frame_type == L2Header::base) {
						found_base_header = true;
						auto *base_header = (L2HeaderBase*) header;
						CPPUNIT_ASSERT( base_header->burst_offset > 0);
					}
				}
				CPPUNIT_ASSERT_EQUAL(true, found_base_header);
			}
		}

		void testContentionMethodNaiveRandomAccess() {
			link_manager->setUseContentionMethod(ContentionMethod::naive_random_access);
			const size_t num_trials = 1000;
			for (size_t i = 0; i < num_trials; i++) {
				// fake that there's nothing scheduled yet
				link_manager->next_broadcast_scheduled = false;
				// notify of new data, triggering the scheduling of a next broadcast slot
				link_manager->notifyOutgoing(128);
				// naive random access picks a random slot from a hard-coded 100 next idle slots
				CPPUNIT_ASSERT(0 < link_manager->next_broadcast_slot);
				CPPUNIT_ASSERT(103 >= link_manager->next_broadcast_slot);
				// make sure that there's just a single broadcast slot scheduled (i.e. the previously scheduled one should've been unscheduled)
				size_t sum = 0;
				for (size_t t = 0; t < planning_horizon; t++)
					sum += link_manager->current_reservation_table->getReservation(t).isIdle() ? 0 : 1;
				CPPUNIT_ASSERT_EQUAL(size_t(1), sum);
			}
		}

		void testContentionMethodAllNeighborsActive() {
			link_manager->setUseContentionMethod(ContentionMethod::all_active_again_assumption);
			int current_slot = 12;
			const size_t max_num_neighbors = 100;
			int previous_num_candidate_slots = 0, first_num_candidate_slots = 0;
			for (size_t n = 0; n < max_num_neighbors; n++) {
				// report the activity of another neighbor
				link_manager->contention_estimator.reportNonBeaconBroadcast(MacId(n+100), current_slot);
				// fake that there's nothing scheduled yet
				link_manager->next_broadcast_scheduled = false;
				// notify of new data, triggering the scheduling of a next broadcast slot
				link_manager->notifyOutgoing(128);
				// the number of candidate slots should be monotonously increasing
				CPPUNIT_ASSERT(link_manager->getNumCandidateSlots(.95) >= previous_num_candidate_slots);
				previous_num_candidate_slots = link_manager->getNumCandidateSlots(.95);
				if (n == 0)
					first_num_candidate_slots = previous_num_candidate_slots;
			}
			// and in the end, more than 10 times as many slots should've been proposed (ensures that the candidate slots don't just stay the same)
			CPPUNIT_ASSERT(previous_num_candidate_slots > 10*first_num_candidate_slots);
		}

		/** Tests binomial estimate which should increase the candidate slots when more neighbors are present. */
		void testContentionMethodBinomialEstimateNoNeighbors() {
			link_manager->setUseContentionMethod(ContentionMethod::binomial_estimate);
			int current_slot = 12;
			const size_t max_num_neighbors = 100;
			int previous_num_candidate_slots = 0, first_num_candidate_slots = 0;
			for (size_t n = 0; n < max_num_neighbors; n++) {
				// report the activity of another neighbor
				link_manager->contention_estimator.reportNonBeaconBroadcast(MacId(n+100), current_slot);
				// fake that there's nothing scheduled yet
				link_manager->next_broadcast_scheduled = false;
				// notify of new data, triggering the scheduling of a next broadcast slot
				link_manager->notifyOutgoing(128);
				// the number of candidate slots should be monotonously increasing
				CPPUNIT_ASSERT(link_manager->getNumCandidateSlots(.95) >= previous_num_candidate_slots);
				previous_num_candidate_slots = link_manager->getNumCandidateSlots(.95);
				if (n == 0)
					first_num_candidate_slots = previous_num_candidate_slots;
			}
			// and in the end, more than 10 times as many slots should've been proposed (ensures that the candidate slots don't just stay the same)
			CPPUNIT_ASSERT(previous_num_candidate_slots > 10*first_num_candidate_slots);
		}

		void testContentionMethodBinomialEstimateIncreasingActivity() {
			link_manager->setUseContentionMethod(ContentionMethod::binomial_estimate);
			int current_slot = 12;
			MacId neighbor_id_1 = MacId(1),
				  neighbor_id_2 = MacId(2),
				  neighbor_id_3 = MacId(3),
				  neighbor_id_4 = MacId(4);
			// No neighbor activity yet, so we expect the minimum no. of candidate slots.
			CPPUNIT_ASSERT_EQUAL(link_manager->MIN_CANDIDATES, link_manager->getNumCandidateSlots(link_manager->broadcast_target_collision_prob));
			// One neighbor broadcasts every slot for 100 slots.
			for (size_t t = 0; t < 100; t++) {
				if (t % 4 == 0)
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_1, t);
				else if (t % 4 == 1)
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_2, t);
				else if (t % 4 == 2)
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_3, t);
				else
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_4, t);
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT(link_manager->getNumCandidateSlots(link_manager->broadcast_target_collision_prob) > link_manager->MIN_CANDIDATES);
		}

		void testContentionMethodPoissonBinomialEstimateIncreasingActivity() {
			link_manager->setUseContentionMethod(ContentionMethod::poisson_binomial_estimate);
			MacId neighbor_id_1 = MacId(1),
			neighbor_id_2 = MacId(2),
			neighbor_id_3 = MacId(3),
			neighbor_id_4 = MacId(4);
			// No neighbor activity yet, so we expect the minimum no. of candidate slots.
			CPPUNIT_ASSERT_EQUAL(link_manager->MIN_CANDIDATES, link_manager->getNumCandidateSlots(link_manager->broadcast_target_collision_prob));
			// Add one active neighbor.
			int current_slot = 0;
			for (size_t t = 0; t < 100; t++, current_slot++) {
				link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_1, t);
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			int previous_num_candidate_slots = link_manager->getNumCandidateSlots(link_manager->broadcast_target_collision_prob);
			CPPUNIT_ASSERT(previous_num_candidate_slots > link_manager->MIN_CANDIDATES);
			// And another
			for (size_t t = 0; t < 100; t++, current_slot++) {
				if (t % 2 == 0)
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_1, t);
				else
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_2, t);
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT(link_manager->getNumCandidateSlots(link_manager->broadcast_target_collision_prob) > previous_num_candidate_slots);
			previous_num_candidate_slots = link_manager->getNumCandidateSlots(link_manager->broadcast_target_collision_prob);
			// And another
			for (size_t t = 0; t < 100; t++, current_slot++) {
				if (t % 3 == 0)
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_1, t);
				else if (t % 3 == 1)
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_2, t);
				else
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_3, t);
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT(link_manager->getNumCandidateSlots(link_manager->broadcast_target_collision_prob) > previous_num_candidate_slots);
			previous_num_candidate_slots = link_manager->getNumCandidateSlots(link_manager->broadcast_target_collision_prob);
			// And another
			for (size_t t = 0; t < 100; t++, current_slot++) {
				if (t % 4 == 0)
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_1, t);
				else if (t % 4 == 1)
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_2, t);
				else if (t % 4 == 2)
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_3, t);
				else
					link_manager->contention_estimator.reportNonBeaconBroadcast(neighbor_id_4, t);
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT(link_manager->getNumCandidateSlots(link_manager->broadcast_target_collision_prob) > previous_num_candidate_slots);
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

		/** Ensures that when slot advertisement is off, the next broadcast slot is not scheduled or advertised if there's no more data to send. */
		void testNoSlotAdvertisement() {
			link_manager->setAlwaysScheduleNextBroadcastSlot(false);
			env->rlc_layer->should_there_be_more_broadcast_data = false;
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
			CPPUNIT_ASSERT_EQUAL(false, link_manager->next_broadcast_scheduled);
			CPPUNIT_ASSERT_EQUAL(uint32_t(0), link_manager->next_broadcast_slot);
			// make sure the header flag hasn't been set in the broadcasted data packet
			auto &outgoing_packets = env->phy_layer->outgoing_packets;
			CPPUNIT_ASSERT_EQUAL(size_t(1), outgoing_packets.size());
			auto &packet = outgoing_packets.at(0);
			CPPUNIT_ASSERT_EQUAL(size_t(2), packet->getHeaders().size());
			const auto &base_header = (L2HeaderBase*) packet->getHeaders().at(0);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, base_header->frame_type);
			CPPUNIT_ASSERT_EQUAL(uint32_t(0), base_header->burst_offset);
		}

		/** Ensures that when slot advertisement is off, the next broadcast slot is scheduled and advertised if there more data to send. */
		void testSlotAdvertisementWhenTheresData() {
			link_manager->setAlwaysScheduleNextBroadcastSlot(false);
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
			CPPUNIT_ASSERT( link_manager->next_broadcast_slot > 0);
			// make sure the header flag has been set in the broadcasted data packet
			auto &outgoing_packets = env->phy_layer->outgoing_packets;
			CPPUNIT_ASSERT_EQUAL(size_t(1), outgoing_packets.size());
			auto &packet = outgoing_packets.at(0);
			CPPUNIT_ASSERT_EQUAL(size_t(2), packet->getHeaders().size());
			const auto &base_header = (L2HeaderBase*) packet->getHeaders().at(0);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, base_header->frame_type);
			CPPUNIT_ASSERT(base_header->burst_offset > 0);
		}

		/** Ensures that when slot advertisement is on, the next broadcast slot is scheduled and advertised if there's no more data to send. */
		void testSlotAdvertisementWhenAutoAdvertisementIsOn() {
			link_manager->setAlwaysScheduleNextBroadcastSlot(true);
			env->rlc_layer->should_there_be_more_broadcast_data = false;
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
			CPPUNIT_ASSERT_EQUAL(size_t(2), packet->getHeaders().size());
			const auto &base_header = (L2HeaderBase*) packet->getHeaders().at(0);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, base_header->frame_type);
			CPPUNIT_ASSERT(base_header->burst_offset > 0);
		}

		/** Ensures that when slot advertisement is on, the next broadcast slot is scheduled and advertised if there's more data to send. */
		void testSlotAdvertisementWhenAutoAdvertisementIsOnAndTheresMoreData() {
			link_manager->setAlwaysScheduleNextBroadcastSlot(true);
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
			CPPUNIT_ASSERT( link_manager->next_broadcast_slot > 0);
			// make sure the header flag has been set in the broadcasted data packet
			auto &outgoing_packets = env->phy_layer->outgoing_packets;
			CPPUNIT_ASSERT_EQUAL(size_t(1), outgoing_packets.size());
			auto &packet = outgoing_packets.at(0);
			CPPUNIT_ASSERT_EQUAL(size_t(2), packet->getHeaders().size());
			const auto &base_header = (L2HeaderBase*) packet->getHeaders().at(0);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, base_header->frame_type);
			CPPUNIT_ASSERT(base_header->burst_offset > 0);
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
		CPPUNIT_TEST(testParseBeaconRescheduleBroadcast);
		CPPUNIT_TEST(testBeaconDestination);
		CPPUNIT_TEST(testDontScheduleNextBroadcastSlot);
		CPPUNIT_TEST(testScheduleNextBroadcastSlotIfTheresData);
		CPPUNIT_TEST(testAutoScheduleBroadcastSlotIfTheresNoData);
		CPPUNIT_TEST(testAutoScheduleBroadcastSlotIfTheresData);
		CPPUNIT_TEST(testContentionMethodNaiveRandomAccess);
		CPPUNIT_TEST(testContentionMethodAllNeighborsActive);
		CPPUNIT_TEST(testContentionMethodBinomialEstimateNoNeighbors);
		CPPUNIT_TEST(testContentionMethodBinomialEstimateIncreasingActivity);
		CPPUNIT_TEST(testContentionMethodPoissonBinomialEstimateIncreasingActivity);
		CPPUNIT_TEST(testAverageBroadcastSlotGenerationMeasurement);
		CPPUNIT_TEST(testNoSlotAdvertisement);
		CPPUNIT_TEST(testSlotAdvertisementWhenTheresData);
		CPPUNIT_TEST(testSlotAdvertisementWhenAutoAdvertisementIsOn);
		CPPUNIT_TEST(testSlotAdvertisementWhenAutoAdvertisementIsOnAndTheresMoreData);

//			CPPUNIT_TEST(testSetBeaconHeader);
//			CPPUNIT_TEST(testProcessIncomingBeacon);
//			CPPUNIT_TEST(testGetNumCandidateSlots);
//			CPPUNIT_TEST(testNotifyOutgoingSingle);
//			CPPUNIT_TEST(testNotifyOutgoingMulti);
		CPPUNIT_TEST_SUITE_END();
	};

}