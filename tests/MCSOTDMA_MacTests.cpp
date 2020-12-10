//
// Created by Sebastian Lindner on 04.12.20.
//

//
// Created by Sebastian Lindner on 04.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../MCSOTDMA_Mac.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class MCSOTDMA_MacTests : public CppUnit::TestFixture {
		private:
			PHYLayer* phy;
			MACLayer* mac;
			ARQLayer* arq;
			RLCLayer* rlc;
			
			uint32_t planning_horizon = 128;
			uint64_t p2p_frequency1 = 1000, p2p_frequency2 = p2p_frequency1 + 180, p2p_frequency3 = p2p_frequency2 + 180, bc_frequency = p2p_frequency2 + 180, bandwidth = 500;
			ReservationManager* reservation_manager;
			MacId communication_partner_id = MacId(42);
			MacId own_id = MacId(41);
		
		public:
			void setUp() override {
				phy = new PHYLayer(planning_horizon);
				mac = new MACLayer(own_id, planning_horizon);
				reservation_manager = mac->reservation_manager;
				reservation_manager->setPhyTransmitterTable(phy->getTransmitterReservationTable());
				reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				reservation_manager->addFrequencyChannel(true, p2p_frequency1, bandwidth);
				reservation_manager->addFrequencyChannel(true, p2p_frequency2, bandwidth);
				reservation_manager->addFrequencyChannel(true, p2p_frequency3, bandwidth);
				// PHY
				mac->setLowerLayer(phy);
				phy->setUpperLayer(mac);
				// ARQ
				arq = new ARQLayer();
				arq->setLowerLayer(mac);
				mac->setUpperLayer(arq);
				// RLC
				rlc = new RLCLayer(own_id);
				arq->setUpperLayer(rlc);
				rlc->setLowerLayer(arq);
			}
			
			void tearDown() override {
				delete mac;
				delete phy;
				delete arq;
				delete rlc;
			}
			
			void testLinkManagerCreation() {
//				coutd.setVerbose(true);
				CPPUNIT_ASSERT_EQUAL(size_t(0), mac->link_managers.size());
				MacId id = MacId(42);
				mac->notifyOutgoing(1024, id);
				CPPUNIT_ASSERT_EQUAL(size_t(1), mac->link_managers.size());
				LinkManager* link_manager = mac->link_managers.at(id);
				CPPUNIT_ASSERT(link_manager);
				CPPUNIT_ASSERT(id == link_manager->getLinkId());
//				coutd.setVerbose(false);
			}
			
			void testMakeReservation() {
				// No injected link request yet.
				CPPUNIT_ASSERT_EQUAL(size_t(0), rlc->injections.size());
				mac->notifyOutgoing(1024, communication_partner_id);
				CPPUNIT_ASSERT(mac->getLinkManager(communication_partner_id)->link_establishment_status == LinkManager::Status::awaiting_reply);
				// Now there should be one.
				CPPUNIT_ASSERT_EQUAL(size_t(1), rlc->injections.size());
			}
			
			void testUpdate() {
				testMakeReservation();
//				coutd.setVerbose(true);
				// No outgoing packets yet.
				CPPUNIT_ASSERT_EQUAL(size_t(0), phy->outgoing_packets.size());
				LinkManager* link_manager = mac->getLinkManager(communication_partner_id);
				link_manager->link_establishment_status = LinkManager::link_established;
				reservation_manager->reservation_tables.at(0)->mark(1, Reservation(communication_partner_id, Reservation::Action::TX));
				mac->update(1);
				// Now there should be one.
				CPPUNIT_ASSERT_EQUAL(size_t(1), phy->outgoing_packets.size());
				CPPUNIT_ASSERT(phy->outgoing_packets.at(0)->getBits() <= phy->getCurrentDatarate());
//				coutd.setVerbose(false);
			}
			
			void testPositions() {
				// Should be able to get your own position.
				bool exception_thrown = false;
				CPRPosition pos = CPRPosition(1, 2, 3, true);
				CPPUNIT_ASSERT(mac->position_map[mac->id] != pos);
				try {
					pos = mac->getPosition(own_id);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
				CPPUNIT_ASSERT(mac->position_map[mac->id] == pos);
				// Shouldn't be able to get some other user's position, who we've never heard of.
				try {
					pos = mac->getPosition(communication_partner_id);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			}
		
		
		CPPUNIT_TEST_SUITE(MCSOTDMA_MacTests);
			CPPUNIT_TEST(testLinkManagerCreation);
			CPPUNIT_TEST(testMakeReservation);
			CPPUNIT_TEST(testUpdate);
			CPPUNIT_TEST(testPositions);
		CPPUNIT_TEST_SUITE_END();
	};
	
}