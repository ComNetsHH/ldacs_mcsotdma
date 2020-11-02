//
// Created by Sebastian Lindner on 14.10.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ReservationTable.hpp"
#include "../ReservationManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

class ReservationTableTests : public CppUnit::TestFixture {
	private:
		ReservationManager* manager;
	
	public:
		void setUp() override {
			uint32_t planning_horizon = 1024;
			manager = new ReservationManager(planning_horizon);
		}
		
		void tearDown() override {
			delete manager;
		}
		
		void testAddFreqChannel() {
		
		}
		
		void testBlacklistFreqChannel() {
		
		}
		
		void testRemoveFreqChannel() {
		
		}
		
		void testMarkSlot() {
		
		}
		
		void testFindIdleGap() {
		
		}
	
	CPPUNIT_TEST_SUITE(ReservationTableTests);
			CPPUNIT_TEST(testAddFreqChannel);
			CPPUNIT_TEST(testBlacklistFreqChannel);
			CPPUNIT_TEST(testRemoveFreqChannel);
			CPPUNIT_TEST(testMarkSlot);
			CPPUNIT_TEST(testFindIdleGap);
		CPPUNIT_TEST_SUITE_END();
};