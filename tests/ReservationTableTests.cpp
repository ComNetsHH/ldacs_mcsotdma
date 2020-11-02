//
// Created by Sebastian Lindner on 06.10.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ReservationTable.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

class ReservationTableTests : public CppUnit::TestFixture {
	private:
		uint32_t planning_horizon;
		ReservationTable* table;
		
	public:
		void setUp() override {
			planning_horizon = 25;
			table = new ReservationTable(planning_horizon);
		}
		
		void tearDown() override {
			delete table;
		}
		
		void testConstructor() {
			bool exception_thrown = false;
			try {
				ReservationTable another_table(UINT32_MAX - 1);
			} catch (const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
			
			try {
				// Shouldn't be able to instantiate a table with that many slots.
				ReservationTable another_table(UINT32_MAX);
			} catch (const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
		}
		
		void testPlanningHorizon() {
			// Planning horizon should be the same as specified.
			CPPUNIT_ASSERT_EQUAL(planning_horizon, table->getPlanningHorizon());
			// Number of slots should be twice the planning horizon plus: once for future, once for past slots, and one for the current slot.
			const std::vector<bool>& vec = table->getVec();
			CPPUNIT_ASSERT_EQUAL(2*planning_horizon+1, (uint32_t) vec.size());
		}
		
		void testValidSlot() {
			// Even though this class is a friend class of ReservationTable, my compiler won't allow me to access protected functions.
			// Hence I will test ReservationTable::isValid(...) through the mark function.
			
			// Entire planning horizon, both negative and positive, should be valid.
			for (int32_t offset = -int32_t(this->planning_horizon); offset <= int32_t(this->planning_horizon); offset++) {
				bool exception_thrown = false;
				try {
					table->mark(offset, true);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
			}
			
			// Going beyond the planning horizon in either direction should be invalid.
			int32_t move_into_invalid_range = 10;
			for (int32_t offset = -int32_t(this->planning_horizon - move_into_invalid_range); offset < -int32_t(this->planning_horizon); offset++) {
				bool exception_thrown = false;
				try {
					table->mark(offset, true);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			}
			for (int32_t offset = int32_t(this->planning_horizon + move_into_invalid_range); offset > int32_t(this->planning_horizon); offset--) {
				bool exception_thrown = false;
				try {
					table->mark(offset, true);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			}
		}
		
		void testValidSlotRange() {
			// We'll consider slot ranges of this length.
			uint32_t range_length = 2;
			// Entire negative range should be valid.
			for (int32_t range_start = -int32_t(this->planning_horizon); range_start <= int32_t(range_length); range_start++) {
				bool exception_thrown = false;
				try {
					table->isUtilized(range_start, range_length);
				} catch (const std::exception& e) {
					exception_thrown = true;
					std::cout << e.what() << std::endl;
				}
				CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
			}
			// Crossing the negative threshold should be valid.
			bool exception_thrown = false;
			try {
				table->isUtilized(-1, range_length);
			} catch (const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
			// Entire positive range should be valid.
			for (int32_t range_start = 0; range_start <= int32_t(this->planning_horizon) - int32_t(range_length); range_start++) {
				exception_thrown = false;
				try {
					table->isUtilized(range_start, range_length);
				} catch (const std::exception& e) {
					exception_thrown = true;
					std::cout << e.what() << std::endl;
				}
				CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
			}
			
			// Starting outside the allowed negative range should be invalid.
			exception_thrown = false;
			try {
				table->isUtilized(planning_horizon - 1, range_length);
			} catch (const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			
			// Going outside the allowed positive range should be invalid.
			exception_thrown = false;
			try {
				table->isUtilized(planning_horizon - 1, range_length);
			} catch (const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
		}
		
		void testMarking() {
			int32_t slot_offset = 0;
			CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(slot_offset));
			table->mark(slot_offset, true);
			CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(slot_offset));
			table->mark(slot_offset, false);
			CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(slot_offset));
			slot_offset = -1;
			table->mark(slot_offset, true);
			CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(slot_offset));
			CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(slot_offset + 1));
			CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(slot_offset + 2));
		}
		
		void testIdleRange() {
			uint32_t length = 2;
			// Negative range.
			for (int32_t start_range = -int32_t(planning_horizon); start_range < -length; start_range++) {
				// Everything should be idle so far.
				CPPUNIT_ASSERT_EQUAL(true, table->isIdle(start_range, length));
				// Mark every second starting point as utilized for the second for-loop.
				if (start_range % 2 == 0)
					table->mark(start_range, true);
			}
			// Everything so far should now be regarded as utilized.
			for (int32_t start_range = -int32_t(planning_horizon); start_range < -length - 1; start_range++) {
				CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(start_range, length));
				if (start_range % 2 == 0)
					CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(start_range));
			}
			
			// Crossing the negative boundary into the positive range should work, too.
			for (int32_t start_range = -length; start_range <= 0; start_range++) {
				CPPUNIT_ASSERT_EQUAL(true, table->isIdle(start_range, length));
				if (start_range % 2 == 0)
					table->mark(start_range, true);
			}
			// Should now be utilized.
			for (int32_t start_range = -length; start_range <= 0; start_range++) {
				CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(start_range, length));
				if (start_range % 2 == 0)
					CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(start_range));
			}
			
			// Positive range.
			for (int32_t start_range = 1; start_range < this->planning_horizon - int32_t(length); start_range++) {
				// Everything should be idle so far.
				CPPUNIT_ASSERT_EQUAL(true, table->isIdle(start_range, length));
				// Mark every second starting point as utilized for the second for-loop.
				if (start_range % 2 == 0)
					table->mark(start_range, true);
			}
			// Everything so far should now be regarded as utilized.
			for (int32_t start_range = 1; start_range < this->planning_horizon - int32_t(length); start_range++) {
				CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(start_range, length));
				if (start_range % 2 == 0)
					CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(start_range));
			}
		}
		
		void testFindIdleRange() {
			int32_t start = int32_t(planning_horizon) - 2;
			// start + length exceeds planning horizon ...
			uint32_t length = 3;
			bool exception_thrown = false;
			try {
				table->findEarliestIdleRange(start, length);
			} catch (const std::invalid_argument& e) {
				exception_thrown = true;
			}
			// ... so an exception should be thrown.
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			
			// Now we can stay 'just' within the valid range.
			length = 2;
			exception_thrown = false;
			try {
				int32_t start_of_idle_range = table->findEarliestIdleRange(start, length);
				CPPUNIT_ASSERT_EQUAL(start, start_of_idle_range);
			} catch (const std::exception& e) {
				exception_thrown = true;
				std::cout << e.what() << std::endl;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
			
			// Starting at 'now' works too.
			start = 0;
			int32_t start_of_idle_range = table->findEarliestIdleRange(start, length);
			CPPUNIT_ASSERT_EQUAL(start, start_of_idle_range);
			
			// But starting in the p
			int32_t idle_slot_range_start = table->findEarliestIdleRange(0, 5);
			CPPUNIT_ASSERT_EQUAL(int32_t(0), idle_slot_range_start);
			table->mark(0, true);
			// x00000
			// 012345
			idle_slot_range_start = table->findEarliestIdleRange(0, 5);
			CPPUNIT_ASSERT_EQUAL(int32_t(1), idle_slot_range_start);
			table->mark(5, true);
			// x0000x0000
			// 0123456789
			idle_slot_range_start = table->findEarliestIdleRange(0, 5);
			CPPUNIT_ASSERT_EQUAL(int32_t(6), idle_slot_range_start);
			table->mark(11, true);
			// x0000x00000x0
			// 0123456789012
			idle_slot_range_start = table->findEarliestIdleRange(0, 5);
			CPPUNIT_ASSERT_EQUAL(int32_t(6), idle_slot_range_start);
			idle_slot_range_start = table->findEarliestIdleRange(7, 5);
			CPPUNIT_ASSERT_EQUAL(int32_t(12), idle_slot_range_start);
		}
	
	CPPUNIT_TEST_SUITE(ReservationTableTests);
		CPPUNIT_TEST(testConstructor);
		CPPUNIT_TEST(testPlanningHorizon);
		CPPUNIT_TEST(testValidSlot);
		CPPUNIT_TEST(testValidSlotRange);
		CPPUNIT_TEST(testMarking);
		CPPUNIT_TEST(testIdleRange);
		CPPUNIT_TEST(testFindIdleRange);
	CPPUNIT_TEST_SUITE_END();
};