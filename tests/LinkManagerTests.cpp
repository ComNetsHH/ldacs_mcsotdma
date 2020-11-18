//
// Created by Sebastian Lindner on 18.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../LinkManager.hpp"
#include "../MCSOTDMA_Mac.hpp"
#include "../coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

class LinkManagerTests : public CppUnit::TestFixture {
	private:
		LinkManager* link_manager;
		ReservationManager* reservation_manager;
		MCSOTDMA_Mac* mac;
		MacId id = MacId(0);
		uint32_t planning_horizon = 128;
	
	public:
		void setUp() override {
			reservation_manager = new ReservationManager(planning_horizon);
			mac = new MCSOTDMA_Mac(reservation_manager);
			link_manager = new LinkManager(id, reservation_manager, mac);
		}
		
		void tearDown() override {
			delete reservation_manager;
			delete mac;
			delete link_manager;
		}
		
		void testTrafficEstimate() {
			CPPUNIT_ASSERT_EQUAL(0.0, link_manager->getCurrentTrafficEstimate());
			unsigned int initial_bits = 10;
			unsigned int num_bits = initial_bits;
			double sum = 0;
			// Fill up the window.
			for (size_t i = 0; i < link_manager->getTrafficEstimateWindowSize(); i++) {
				link_manager->notifyOutgoing(num_bits);
				sum += num_bits;
				num_bits += initial_bits;
				CPPUNIT_ASSERT_EQUAL(sum / (i+1), link_manager->getCurrentTrafficEstimate());
			}
			// Now it's full, so the next input will kick out the first value.
			link_manager->notifyOutgoing(num_bits);
			sum -= initial_bits;
			sum += num_bits;
			CPPUNIT_ASSERT_EQUAL(sum / (link_manager->getTrafficEstimateWindowSize()), link_manager->getCurrentTrafficEstimate());
		}
	
	CPPUNIT_TEST_SUITE(LinkManagerTests);
		CPPUNIT_TEST(testTrafficEstimate);
	CPPUNIT_TEST_SUITE_END();
};