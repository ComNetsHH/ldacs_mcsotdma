//
// Created by Sebastian Lindner on 10.12.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class SystemTests : public CppUnit::TestFixture {
		private:
			MacId own_id = MacId(42);
			MacId communication_partner_id = MacId(43);
			uint32_t planning_horizon = 128;
			uint64_t center_frequency1 = 1000, center_frequency2 = 2000, center_frequency3 = 3000, bc_frequency = 4000, bandwidth = 500;
			MACLayer* mac;
			ARQLayer* arq_layer;
			RLCLayer* rlc_layer;
			PHYLayer* phy_layer;
			NetworkLayer* net_layer;
		
		public:
			void setUp() override {
				phy_layer = new PHYLayer(planning_horizon);
				mac = new MACLayer(own_id, planning_horizon);
				mac->reservation_manager->setPhyTransmitterTable(phy_layer->getTransmitterReservationTable());
				mac->reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				mac->reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				mac->reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				mac->reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				
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
				delete arq_layer;
				delete rlc_layer;
				delete phy_layer;
				delete net_layer;
			}
			
			void testStartOperation() {
				coutd.setVerbose(true);
				mac->notifyOutgoing(512, communication_partner_id);
				
				coutd.setVerbose(false);
			}
			
		
		CPPUNIT_TEST_SUITE(SystemTests);
			CPPUNIT_TEST(testStartOperation);
		CPPUNIT_TEST_SUITE_END();
	};
	
}