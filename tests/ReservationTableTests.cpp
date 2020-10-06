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
			uint32_t start = 0, end = 3;
			std::vector<bool> status_vec = table->isIdle(start, end);
			CPPUNIT_ASSERT_EQUAL((unsigned long) (end - start), status_vec.size());
			for (size_t i = start; i < end; i++) {
				if (i == 0)
					CPPUNIT_ASSERT_EQUAL(true, (bool) status_vec.at(i));
				else
					CPPUNIT_ASSERT_EQUAL(false, (bool) status_vec.at(i));
			}
		}
		
		void testFindIdleRange() {
			uint32_t idle_slot_range_start = table->findEarliestIdleRange(0, 5);
			CPPUNIT_ASSERT_EQUAL(uint32_t(0), idle_slot_range_start);
			table->mark(0, true);
			// x00000
			// 012345
			idle_slot_range_start = table->findEarliestIdleRange(0, 5);
			CPPUNIT_ASSERT_EQUAL(uint32_t(1), idle_slot_range_start);
			table->mark(5, true);
			// x0000x0000
			// 0123456789
			idle_slot_range_start = table->findEarliestIdleRange(0, 5);
			CPPUNIT_ASSERT_EQUAL(uint32_t(6), idle_slot_range_start);
			table->mark(11, true);
			// x0000x00000x0
			// 0123456789012
			idle_slot_range_start = table->findEarliestIdleRange(0, 5);
			CPPUNIT_ASSERT_EQUAL(uint32_t(6), idle_slot_range_start);
			idle_slot_range_start = table->findEarliestIdleRange(7, 5);
			CPPUNIT_ASSERT_EQUAL(uint32_t(12), idle_slot_range_start);
		}
	
	CPPUNIT_TEST_SUITE(ReservationTableTests);
		CPPUNIT_TEST(testIsIdle);
		CPPUNIT_TEST(testFindIdleRange);
	CPPUNIT_TEST_SUITE_END();
};