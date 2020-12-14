//
// Created by Sebastian Lindner on 10.12.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"
#include "../BCLinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class SystemTests : public CppUnit::TestFixture {
		private:
			MacId own_id = MacId(42);
			MacId communication_partner_id = MacId(43);
			uint32_t planning_horizon = 256;
			uint64_t center_frequency1 = 1000, center_frequency2 = 2000, center_frequency3 = 3000, bc_frequency = 4000, bandwidth = 500;
			MACLayer *mac_layer_me, *mac_layer_you;
			ARQLayer *arq_layer_me, *arq_layer_you;
			RLCLayer *rlc_layer_me, *rlc_layer_you;
			PHYLayer *phy_layer_me, *phy_layer_you;
			NetworkLayer *net_layer_me, *net_layer_you;
		
		public:
			void setUp() override {
				phy_layer_me = new PHYLayer(planning_horizon);
				mac_layer_me = new MACLayer(own_id, planning_horizon);
				mac_layer_me->reservation_manager->setPhyTransmitterTable(phy_layer_me->getTransmitterReservationTable());
				mac_layer_me->reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				mac_layer_me->reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				mac_layer_me->reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				mac_layer_me->reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				
				arq_layer_me = new ARQLayer();
				arq_layer_me->should_forward = true;
				mac_layer_me->setUpperLayer(arq_layer_me);
				arq_layer_me->setLowerLayer(mac_layer_me);
				net_layer_me = new NetworkLayer();
				rlc_layer_me = new RLCLayer(own_id);
				net_layer_me->setLowerLayer(rlc_layer_me);
				rlc_layer_me->setUpperLayer(net_layer_me);
				rlc_layer_me->setLowerLayer(arq_layer_me);
				arq_layer_me->setUpperLayer(rlc_layer_me);
				phy_layer_me->setUpperLayer(mac_layer_me);
				mac_layer_me->setLowerLayer(phy_layer_me);
				
				phy_layer_you = new PHYLayer(planning_horizon);
				mac_layer_you = new MACLayer(communication_partner_id, planning_horizon);
				mac_layer_you->reservation_manager->setPhyTransmitterTable(phy_layer_you->getTransmitterReservationTable());
				mac_layer_you->reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				mac_layer_you->reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				mac_layer_you->reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				mac_layer_you->reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				
				arq_layer_you = new ARQLayer();
				arq_layer_you->should_forward = true;
				mac_layer_you->setUpperLayer(arq_layer_you);
				arq_layer_you->setLowerLayer(mac_layer_you);
				net_layer_you = new NetworkLayer();
				rlc_layer_you = new RLCLayer(own_id);
				net_layer_you->setLowerLayer(rlc_layer_you);
				rlc_layer_you->setUpperLayer(net_layer_you);
				rlc_layer_you->setLowerLayer(arq_layer_you);
				arq_layer_you->setUpperLayer(rlc_layer_you);
				phy_layer_you->setUpperLayer(mac_layer_you);
				mac_layer_you->setLowerLayer(phy_layer_you);
				
				phy_layer_me->connected_phy = phy_layer_you;
				phy_layer_you->connected_phy = phy_layer_me;
			}
			
			void tearDown() override {
				delete mac_layer_me;
				delete arq_layer_me;
				delete rlc_layer_me;
				delete phy_layer_me;
				delete net_layer_me;
				
				delete mac_layer_you;
				delete arq_layer_you;
				delete rlc_layer_you;
				delete phy_layer_you;
				delete net_layer_you;
			}
			
			void testBroadcast() {
//				coutd.setVerbose(true);
				rlc_layer_me->should_there_be_more_data = false;
				CPPUNIT_ASSERT_EQUAL(size_t(0), rlc_layer_you->receptions.size());
				mac_layer_me->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				while (((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->broadcast_slot_scheduled) {
					mac_layer_me->update(1);
					mac_layer_you->update(1);
					mac_layer_me->execute();
					mac_layer_you->execute();
				}
				CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
//				coutd.setVerbose(false);
			}
			
			void testLinkEstablishment() {
//				coutd.setVerbose(true);
				rlc_layer_me->should_there_be_more_data = false;
				// New data for partner.
				mac_layer_me->notifyOutgoing(512, communication_partner_id);
				while (((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->broadcast_slot_scheduled) {
					// Order is important: if 'you' updates last, the reply may already be sent, and we couldn't check the next condition (or check for both awaiting_reply OR established).
					mac_layer_you->update(1);
					mac_layer_me->update(1);
					mac_layer_me->execute();
					mac_layer_you->execute();
				}
				// Link request should've been sent, so we're awaiting_reply.
				CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, mac_layer_me->getLinkManager(communication_partner_id)->link_establishment_status);
				while (mac_layer_me->getLinkManager(communication_partner_id)->link_establishment_status != LinkManager::link_established) {
					mac_layer_me->update(1);
					mac_layer_you->update(1);
					mac_layer_me->execute();
					mac_layer_you->execute();
				}
				// Link reply should've arrived, so link should be established.
				CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_me->getLinkManager(communication_partner_id)->link_establishment_status);
				// Make sure that all corresponding slots are marked as TX on our side.
				LinkManager* lm_me = mac_layer_me->getLinkManager(communication_partner_id);
				ReservationTable* table_me = lm_me->current_reservation_table;
				for (int offset = lm_me->tx_offset; offset < lm_me->tx_timeout * lm_me->tx_offset; offset += lm_me->tx_offset) {
					const Reservation& reservation = table_me->getReservation(offset);
					CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, reservation.getAction());
					CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation.getTarget());
				}
				// Make sure that the same slots are marked as RX on their side.
				LinkManager* lm_you = mac_layer_you->getLinkManager(own_id);
				ReservationTable* table_you = lm_you->current_reservation_table;
				std::vector<int> reserved_time_slots;
				for (int offset = lm_me->tx_offset; offset < lm_me->tx_timeout * lm_me->tx_offset; offset += lm_me->tx_offset) {
					const Reservation& reservation = table_you->getReservation(offset);
					CPPUNIT_ASSERT_EQUAL(own_id, reservation.getTarget());
					CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, reservation.getAction());
					reserved_time_slots.push_back(offset);
				}
				CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
				// Wait until the next transmission.
				for (size_t i = 0; i < lm_you->tx_offset; i++) {
					mac_layer_me->update(1);
					mac_layer_you->update(1);
					mac_layer_me->execute();
					mac_layer_you->execute();
				}
				CPPUNIT_ASSERT_EQUAL(size_t(2), rlc_layer_you->receptions.size());
				// Ensure reservations are still valid.
				for (size_t i = 0; i < reserved_time_slots.size(); i++) {
					int offset = reserved_time_slots.at(i);
					const Reservation& reservation = table_you->getReservation(offset - lm_you->tx_offset); // Normalize saved offsets to current time
					CPPUNIT_ASSERT_EQUAL(own_id, reservation.getTarget());
					CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, reservation.getAction());
					// All inbetween current and next reservation should be IDLE.
					if (i < reserved_time_slots.size() - 1) {
						int next_offset = reserved_time_slots.at(i+1);
						for (int j = offset + 1; j < next_offset; j++) {
							const Reservation& next_reservation = table_you->getReservation(j);
							CPPUNIT_ASSERT_EQUAL(SYMBOLIC_ID_UNSET, next_reservation.getTarget());
							CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, next_reservation.getAction());
						}
						coutd << std::endl;
					} else {
						for (int j = reserved_time_slots.at(reserved_time_slots.size() - 1) + 1; j < planning_horizon; j++) {
							const Reservation& next_reservation = table_you->getReservation(j);
							CPPUNIT_ASSERT_EQUAL(SYMBOLIC_ID_UNSET, next_reservation.getTarget());
							CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, next_reservation.getAction());
						}
					}
				}
//				coutd.setVerbose(false);
			}
			
			// TODO
			void testEncapsulatedUnicast() {
			}
			
			// TODO
			void testLinkRenewal() {
			}
			
			// TODO
			void testRequestDoesntMakeIt() {
			}
			
			// TODO
			void testReplyDoesntMakeIt() {
			
			}
		
		CPPUNIT_TEST_SUITE(SystemTests);
			CPPUNIT_TEST(testBroadcast);
			CPPUNIT_TEST(testLinkEstablishment);
//			CPPUNIT_TEST(testEncapsulatedUnicast);
//			CPPUNIT_TEST(testLinkRenewal);
		CPPUNIT_TEST_SUITE_END();
	};
	
}