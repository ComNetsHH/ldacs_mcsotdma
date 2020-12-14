//
// Created by Sebastian Lindner on 09.12.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <L2Packet.hpp>
#include "../FrequencyChannel.hpp"
#include "../Reservation.hpp"
#include "../ReservationTable.hpp"
#include "../ReservationManager.hpp"
#include "../BCLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class BCLinkManagerTests : public CppUnit::TestFixture {
		private:
			BCLinkManager* link_manager;
			ReservationManager* reservation_manager;
			MacId own_id = MacId(42);
			MacId communication_partner_id = MacId(43);
			uint32_t planning_horizon = 128;
			uint64_t center_frequency1 = 1000, center_frequency2 = 2000, center_frequency3 = 3000, bc_frequency = 4000, bandwidth = 500;
			unsigned long num_bits_going_out = 800*100;
			MACLayer* mac;
			ARQLayer* arq_layer;
			RLCLayer* rlc_layer;
			PHYLayer* phy_layer;
			NetworkLayer* net_layer;
		
		public:
			void setUp() override {
				phy_layer = new PHYLayer(planning_horizon);
				mac = new MACLayer(own_id, planning_horizon);
				reservation_manager = mac->reservation_manager;
				reservation_manager->setPhyTransmitterTable(phy_layer->getTransmitterReservationTable());
				reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				link_manager = new BCLinkManager(SYMBOLIC_LINK_ID_BROADCAST, reservation_manager, mac);
				arq_layer = new ARQLayer();
				mac->setUpperLayer(arq_layer);
				arq_layer->setLowerLayer(mac);
				net_layer = new NetworkLayer();
				rlc_layer = new RLCLayer(own_id);
				net_layer->setLowerLayer(rlc_layer);
				rlc_layer->setUpperLayer(net_layer);
				rlc_layer->setLowerLayer(arq_layer);
				arq_layer->setUpperLayer(rlc_layer);
				phy_layer->setUpperLayer(mac);
				mac->setLowerLayer(phy_layer);
				
			}
			
			void tearDown() override {
				delete mac;
				delete link_manager;
				delete arq_layer;
				delete rlc_layer;
				delete phy_layer;
				delete net_layer;
			}
			
			void testSetBeaconHeader() {
				L2HeaderBeacon header = L2HeaderBeacon();
				link_manager->setHeaderFields(&header);
				unsigned int num_hops = net_layer->getNumHopsToGroundStation();
				CPPUNIT_ASSERT_EQUAL(num_hops, header.num_hops_to_ground_station);
			}
			
			void testProcessIncomingBeacon() {
				Reservation reservation = Reservation(own_id, Reservation::TX);
				ReservationTable* tbl = reservation_manager->getReservationTableByIndex(0);
				tbl->mark(3, reservation);
				CPPUNIT_ASSERT(tbl->getReservation(3) == reservation);
				// This beacon should encapsulate the just-made reservation.
				L2Packet* beacon = link_manager->prepareBeacon();
				// Let's undo the reservation.
				tbl->mark(3, Reservation());
				CPPUNIT_ASSERT(tbl->getReservation(3) != reservation);
				// Now we receive the beacon.
				auto* beacon_header = (L2HeaderBeacon*) beacon->getHeaders().at(1);
				auto* beacon_payload = (BeaconPayload*) beacon->getPayloads().at(1);
				link_manager->processIncomingBeacon(MacId(10), beacon_header, beacon_payload);
				// So the reservation should be made again.
				CPPUNIT_ASSERT(tbl->getReservation(3) == reservation);
				delete beacon;
			}
			
			void testGetNumCandidateSlots() {
				link_manager->contention_estimator.reportBroadcast(communication_partner_id);
				link_manager->contention_estimator.update();
				link_manager->contention_estimator.update();
				// 50% broadcast rate of the only neighbor
				CPPUNIT_ASSERT_EQUAL(.5, link_manager->contention_estimator.getAverageBroadcastRate());
				// 50% target collision probability
				double target_collision_prob = .5;
				uint expected_num_slots = 2; // => Picking 1 slot out of 2 has a collision probability of 50% then.
				CPPUNIT_ASSERT_EQUAL(expected_num_slots, link_manager->getNumCandidateSlots(target_collision_prob));
				// 5% target collision probability
				target_collision_prob = .05;
				expected_num_slots = 11; // comparing to MATLAB implementation
				CPPUNIT_ASSERT_EQUAL(expected_num_slots, link_manager->getNumCandidateSlots(target_collision_prob));
				MacId other_id = MacId(communication_partner_id.getId() + 1);
				link_manager->contention_estimator.reportBroadcast(communication_partner_id);
				link_manager->contention_estimator.reportBroadcast(other_id);
				link_manager->contention_estimator.update();
				link_manager->contention_estimator.update();
				// 50% broadcast rate of *two* neighbors
				CPPUNIT_ASSERT_EQUAL(.5, link_manager->contention_estimator.getAverageBroadcastRate());
				expected_num_slots = 21; // comparing to MATLAB implementation
				CPPUNIT_ASSERT_EQUAL(expected_num_slots, link_manager->getNumCandidateSlots(target_collision_prob));
			}
			
			void testNotifyOutgoingSingle() {
//				coutd.setVerbose(true);
				mac->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				Reservation reservation = mac->reservation_manager->getBroadcastReservationTable()->getReservation(1);
				CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, reservation.getAction());
				CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, reservation.getTarget());
				// So that querying whether there's more data returns false -> no next broadcast
				rlc_layer->should_there_be_more_data = false;
				CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
				while (((BCLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->broadcast_slot_scheduled)
					mac->update(1);
				CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());
				L2Packet* packet = phy_layer->outgoing_packets.at(0);
				CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, packet->getDestination());
				auto* base_header = (L2HeaderBase*) packet->getHeaders().at(0);
				CPPUNIT_ASSERT_EQUAL(own_id, base_header->icao_id);
				CPPUNIT_ASSERT_EQUAL(own_id, packet->getOrigin());
				CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, packet->getDestination());
				CPPUNIT_ASSERT_EQUAL(ushort(1), base_header->length_next);
				CPPUNIT_ASSERT_EQUAL(uint(0), base_header->offset);
				CPPUNIT_ASSERT_EQUAL(uint(0), base_header->timeout);
//				coutd.setVerbose(false);
			}
			
			void testNotifyOutgoingMulti() {
//				coutd.setVerbose(true);
				mac->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				Reservation reservation = mac->reservation_manager->getBroadcastReservationTable()->getReservation(1);
				CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, reservation.getAction());
				CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, reservation.getTarget());
				// So that a next broadcast must be scheduled.
				rlc_layer->should_there_be_more_data = true;
				CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
				while (phy_layer->outgoing_packets.empty())
					mac->update(1);
				CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());
				L2Packet* packet = phy_layer->outgoing_packets.at(0);
				CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, packet->getDestination());
				auto* base_header = (L2HeaderBase*) packet->getHeaders().at(0);
				CPPUNIT_ASSERT_EQUAL(ushort(1), base_header->length_next);
				// A non-zero offset means we must've scheduled a next broadcast.
				CPPUNIT_ASSERT(base_header->offset > 0);
				CPPUNIT_ASSERT_EQUAL(uint(0), base_header->timeout);
//				coutd.setVerbose(false);
			}
		
		CPPUNIT_TEST_SUITE(BCLinkManagerTests);
			CPPUNIT_TEST(testSetBeaconHeader);
			CPPUNIT_TEST(testProcessIncomingBeacon);
			CPPUNIT_TEST(testGetNumCandidateSlots);
			CPPUNIT_TEST(testNotifyOutgoingSingle);
			CPPUNIT_TEST(testNotifyOutgoingMulti);
		CPPUNIT_TEST_SUITE_END();
	};
	
}