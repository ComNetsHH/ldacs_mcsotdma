//
// Created by Sebastian Lindner on 06.10.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ReservationTable.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

class ReservationTableTests : public CppUnit::TestFixture {
	private:
		ReservationTable* table;
		
	public:
		void setUp() override {
			uint32_t planning_horizon = 1024;
			table = new ReservationTable(planning_horizon);
		}
		
		void tearDown() override {
			delete table;
		}
		
		void testIsIdle() {
			CPPUNIT_ASSERT_EQUAL(false, table->isIdle(0));
			table->mark(0, true);
			CPPUNIT_ASSERT_EQUAL(true, table->isIdle(0));
		}
	
	CPPUNIT_TEST_SUITE(ReservationTableTests);
		CPPUNIT_TEST(testIsIdle);
	CPPUNIT_TEST_SUITE_END();
};