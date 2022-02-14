//
// Created by Sebastian Lindner on 09.12.20.
//

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
		MACLayer *mac;
		TestEnvironment *env;

	public:
		void setUp() override {
			id = MacId(42);
			partner_id = MacId(43);
			env = new TestEnvironment(id, partner_id);
			planning_horizon = env->planning_horizon;
			mac = env->mac_layer;
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
			CPPUNIT_ASSERT(link_manager->next_broadcast_slot >= uint32_t(1) && link_manager->next_broadcast_slot <= link_manager->MIN_CANDIDATES);
		}

		void testBroadcast() {
			link_manager->notifyOutgoing(512);
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

		void testScheduleNextBeacon() {
			size_t num_slots = 0, max_slots = link_manager->beacon_module.min_beacon_offset*5;
			while (mac->stat_num_beacons_sent.get() < 2 && num_slots++ < max_slots) {			
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();				
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(2), (size_t) mac->stat_num_beacons_sent.get());
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
			auto beacon_msg = ((SHLinkManager*) env_you.mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->beacon_module.generateBeacon(manager->getP2PReservationTables(), manager->getBroadcastReservationTable(), mac->getHostPosition(), mac->getNumUtilizedP2PResources(), mac->getP2PBurstOffset());
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
			auto pair = ((SHLinkManager*) env_you.mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->beacon_module.generateBeacon({}, bc_table_you, mac->getHostPosition(), mac->getNumUtilizedP2PResources(), mac->getP2PBurstOffset());

			auto* bc_lm = (SHLinkManager*) env->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			bc_lm->beacon_module.next_beacon_in = t;
			bc_lm->current_reservation_table->mark(t, Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON));
			bc_lm->next_beacon_scheduled = true;
			CPPUNIT_ASSERT(bc_lm->beacon_module.next_beacon_in == t);
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON), bc_lm->current_reservation_table->getReservation(t));
			bc_lm->processBeaconMessage(partner_id, pair.first, pair.second);
			CPPUNIT_ASSERT(bc_lm->beacon_module.next_beacon_in > t);
			CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), bc_lm->current_reservation_table->getReservation(t));
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON), bc_lm->current_reservation_table->getReservation(bc_lm->beacon_module.next_beacon_in));
		}

		/**
		 * If user1 has scheduled a broadcast transmission during a slot that is utilized by another user, as it learns by parsing that user's beacon, it should re-schedule its own broadcast transmission.
		 */
		void testParseBeaconRescheduleBroadcast() {
			// schedule some broadcast slot
			auto* bc_lm = (SHLinkManager*) env->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			bc_lm->scheduleBroadcastSlot();
			// which turned out to be 't'
			int t = (int) bc_lm->next_broadcast_slot;
			CPPUNIT_ASSERT(t > 0);

			TestEnvironment env_you = TestEnvironment(partner_id, id);
			ReservationTable *bc_table_you = env_you.mac_layer->reservation_manager->getBroadcastReservationTable();
			// now have another user schedule its broadcast also at 't'
			bc_table_you->mark(t, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX));
			// which will be notified to the first user through a beacon
			auto pair = ((SHLinkManager*) env_you.mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->beacon_module.generateBeacon({}, bc_table_you, mac->getHostPosition(), mac->getNumUtilizedP2PResources(), mac->getP2PBurstOffset());

			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX), bc_lm->current_reservation_table->getReservation(t));
			// which is processed
			bc_lm->processBeaconMessage(partner_id, pair.first, pair.second);
			// and now the first user should've moved away from 't'
			CPPUNIT_ASSERT(bc_lm->next_broadcast_slot != t);
			// and marked the slot as RX
			CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), bc_lm->current_reservation_table->getReservation(t));
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX), bc_lm->current_reservation_table->getReservation(bc_lm->next_broadcast_slot));
		}

		void testBeaconDestination() {
			auto *packet = new L2Packet();
			auto *base_header = new L2HeaderBase(MacId(42), 0, 1, 1, 0);
			packet->addMessage(base_header, nullptr);
			packet->addMessage(link_manager->beacon_module.generateBeacon(link_manager->reservation_manager->getP2PReservationTables(), link_manager->reservation_manager->getBroadcastReservationTable(), mac->getHostPosition(), mac->getNumUtilizedP2PResources(), mac->getP2PBurstOffset()));
			CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BEACON, packet->getDestination());
		}

		void testDontScheduleNextBroadcastSlot() {
			// don't auto-schedule a next slot => only do so if there's more data.
			link_manager->setAlwaysScheduleNextBroadcastSlot(false);
			// don't generate new broadcast data.
			env->rlc_layer->should_there_be_more_broadcast_data = false;
			env->rlc_layer->num_remaining_broadcast_packets = 1;
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
			CPPUNIT_ASSERT(env->phy_layer->outgoing_packets.size() > 1);
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

		void testMacDelay() {
			// give it some data to send
			env->rlc_layer->should_there_be_more_broadcast_data = true;
			link_manager->notifyOutgoing(512);						
			std::vector<double> delays;
			size_t num_slots = 0, max_num_slots = 100, num_tx = 10;
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
				CPPUNIT_ASSERT_LESSEQUAL((double) link_manager->MIN_CANDIDATES, d);
			}
		}

		void testBeaconInterval() {
			link_manager->beacon_module.setEnabled(false);

			size_t target_num_neighbors = 19;
			for (size_t n = 0; n < target_num_neighbors; n++) {
				link_manager->onSlotStart(1);
				auto *beacon_packet = new L2Packet();
				beacon_packet->addMessage(new L2HeaderBase(MacId(100 + n), 0, 0, 0, 0), nullptr);
				beacon_packet->addMessage(new L2HeaderBeacon(), nullptr);
				mac->receiveFromLower(beacon_packet, env->sh_frequency);
				mac->onSlotEnd();
				CPPUNIT_ASSERT_EQUAL(n+1, mac->getNeighborObserver().getNumActiveNeighbors());
			}		

			CPPUNIT_ASSERT_EQUAL(link_manager->beacon_module.min_beacon_offset, link_manager->beacon_module.getBeaconOffset());
			CPPUNIT_ASSERT_EQUAL(false, link_manager->next_beacon_scheduled);
			link_manager->beacon_module.setEnabled(true);
			link_manager->scheduleBeacon();
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_beacon_scheduled);			
			CPPUNIT_ASSERT_GREATER(link_manager->beacon_module.min_beacon_offset, link_manager->beacon_module.getBeaconOffset());
		}

		void testBeaconWithResourceUtilization() {
			// enable writing resource utilization into beacon
			link_manager->setWriteResourceUtilizationIntoBeacon(true);
			// transmit a beacon
			size_t num_slots = 0, max_slots = 1000;
			while (mac->stat_num_beacons_sent.get() < 1.0 && num_slots++ < max_slots) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_beacons_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());			
			L2Packet *beacon = env->phy_layer->outgoing_packets.at(0);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::beacon, beacon->getHeaders().at(1)->frame_type);
			BeaconPayload *payload = (BeaconPayload*) beacon->getPayloads().at(1);
			CPPUNIT_ASSERT(payload != nullptr);
			CPPUNIT_ASSERT_GREATER(size_t(0), payload->local_reservations.size());
		}

		void testBeaconWithoutResourceUtilization() {
			// disable writing resource utilization into beacon
			link_manager->setWriteResourceUtilizationIntoBeacon(false);
			// transmit a beacon
			size_t num_slots = 0, max_slots = 1000;
			while (mac->stat_num_beacons_sent.get() < 1.0 && num_slots++ < max_slots) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_beacons_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());			
			L2Packet *beacon = env->phy_layer->outgoing_packets.at(0);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::beacon, beacon->getHeaders().at(1)->frame_type);
			BeaconPayload *payload = (BeaconPayload*) beacon->getPayloads().at(1);
			CPPUNIT_ASSERT(payload != nullptr);
			CPPUNIT_ASSERT_EQUAL(size_t(0), payload->local_reservations.size());
		}

		void testSHChannelAccessDelay() {
			const double target_collision_prob = .626;
			// zero neighbors
			double n = 0;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			unsigned int k = link_manager->getNumCandidateSlots(target_collision_prob);			
			unsigned int expected_k = std::max((double) link_manager->MIN_CANDIDATES, 2.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, k);						
			// one neighbor
			mac->reportNeighborActivity(MacId(1));
			n = 1;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			unsigned int new_k = link_manager->getNumCandidateSlots(target_collision_prob);			
			expected_k = std::ceil(2.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATER(k, new_k);
			k = new_k;
			// two neighbors
			mac->reportNeighborActivity(MacId(2));
			n = 2;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			new_k = link_manager->getNumCandidateSlots(target_collision_prob);			
			expected_k = std::ceil(2.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATER(k, new_k);
			k = new_k;
			// three neighbors
			mac->reportNeighborActivity(MacId(3));
			n = 3;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			new_k = link_manager->getNumCandidateSlots(target_collision_prob);			
			expected_k = std::ceil(2.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATER(k, new_k);
			k = new_k;
			// four neighbors
			mac->reportNeighborActivity(MacId(4));
			n = 4;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());
			new_k = link_manager->getNumCandidateSlots(target_collision_prob);			
			expected_k = std::ceil(2.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATER(k, new_k);
			k = new_k;
			// five neighbors
			mac->reportNeighborActivity(MacId(5));
			n = 5;
			CPPUNIT_ASSERT_EQUAL(size_t(n), mac->getNeighborObserver().getNumActiveNeighbors());	
			new_k = link_manager->getNumCandidateSlots(target_collision_prob);			
			expected_k = std::ceil(2.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
			CPPUNIT_ASSERT_EQUAL(expected_k, new_k);
			CPPUNIT_ASSERT_GREATER(k, new_k);
			k = new_k;
		}

		/** During simulations, a maximum no. of candidate slots was observed, which didn't make much sense. */
		void testNoCandidateSlotsForParticularValues() {
			size_t num_neighbors = 30;
			for (size_t i = 0; i < num_neighbors; i++)
				mac->reportNeighborActivity(MacId(i));
			CPPUNIT_ASSERT_EQUAL(num_neighbors, mac->getNeighborObserver().getNumActiveNeighbors());
			double target_collision_prob = 0.05;			
			CPPUNIT_ASSERT_GREATER(uint(1000), link_manager->getNumCandidateSlots(target_collision_prob));
		}

		/** #111 when no next beacon slot can be found, it should try again every slot until it can. */
		void testCannotScheduleBeaconSlot() {
			// mark everything as busy
			for (int t = 0; t < planning_horizon; t++) 
				link_manager->current_reservation_table->mark(t, Reservation(MacId(1), Reservation::BUSY));
			// trying to schedule a beacon should throw an error
			CPPUNIT_ASSERT_THROW(link_manager->beacon_module.scheduleNextBeacon(0.0, 0, link_manager->current_reservation_table, mac->reservation_manager->getTxTable()), std::runtime_error);			
			// update one slot
			mac->update(1);
			link_manager->current_reservation_table->mark(planning_horizon - 1, Reservation(MacId(1), Reservation::BUSY)); // still nothing free
			mac->execute();
			mac->onSlotEnd();
			// shouldn't have succeeded in scheduling a beacon
			CPPUNIT_ASSERT_EQUAL(false, link_manager->next_beacon_scheduled);
			// update another slot, where the newest slot will be free
			mac->update(1);
			mac->execute();
			mac->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(true, link_manager->next_beacon_scheduled);									
			CPPUNIT_ASSERT_EQUAL(planning_horizon - 2, link_manager->beacon_module.getNextBeaconSlot()); // -2 because it's already been decremented once
		}

	CPPUNIT_TEST_SUITE(SHLinkManagerTests);
		CPPUNIT_TEST(testBroadcastSlotSelection);
		CPPUNIT_TEST(testScheduleBroadcastSlot);
		CPPUNIT_TEST(testBroadcast);
		CPPUNIT_TEST(testSendLinkRequestOnBC);		
		CPPUNIT_TEST(testScheduleNextBeacon);
		CPPUNIT_TEST(testParseBeacon);
		CPPUNIT_TEST(testParseBeaconRescheduleBeacon);
		CPPUNIT_TEST(testParseBeaconRescheduleBroadcast);
		CPPUNIT_TEST(testBeaconDestination);
		CPPUNIT_TEST(testDontScheduleNextBroadcastSlot);
		CPPUNIT_TEST(testScheduleNextBroadcastSlotIfTheresData);
		CPPUNIT_TEST(testAutoScheduleBroadcastSlotIfTheresNoData);
		CPPUNIT_TEST(testAutoScheduleBroadcastSlotIfTheresData);
		CPPUNIT_TEST(testAverageBroadcastSlotGenerationMeasurement);
		CPPUNIT_TEST(testNoSlotAdvertisement);
		CPPUNIT_TEST(testSlotAdvertisementWhenTheresData);
		CPPUNIT_TEST(testSlotAdvertisementWhenAutoAdvertisementIsOn);
		CPPUNIT_TEST(testSlotAdvertisementWhenAutoAdvertisementIsOnAndTheresMoreData);
		CPPUNIT_TEST(testMacDelay);
		CPPUNIT_TEST(testBeaconInterval);				
		CPPUNIT_TEST(testBeaconWithResourceUtilization);	
		CPPUNIT_TEST(testBeaconWithoutResourceUtilization);	
		CPPUNIT_TEST(testSHChannelAccessDelay);	
		CPPUNIT_TEST(testNoCandidateSlotsForParticularValues);	
		CPPUNIT_TEST(testCannotScheduleBeaconSlot);			

//			CPPUNIT_TEST(testSetBeaconHeader);
//			CPPUNIT_TEST(testProcessIncomingBeacon);
//			CPPUNIT_TEST(testGetNumCandidateSlots);
//			CPPUNIT_TEST(testNotifyOutgoingSingle);
//			CPPUNIT_TEST(testNotifyOutgoingMulti);
		CPPUNIT_TEST_SUITE_END();
	};

}
