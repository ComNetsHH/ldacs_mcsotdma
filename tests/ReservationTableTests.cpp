//
// Created by Sebastian Lindner on 06.10.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ReservationTable.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
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
					ReservationTable another_table = ReservationTable(2);
				} catch (const std::exception& e) {
					std::cout << e.what() << std::endl;
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
			}
			
			void testPlanningHorizon() {
				// Planning horizon should be the same as specified.
				CPPUNIT_ASSERT_EQUAL(planning_horizon, table->getPlanningHorizon());
				// Number of slots should be twice the planning horizon plus: once for future, once for past slots, and one for the current slot.
				const std::vector<Reservation>& vec = table->getVec();
				CPPUNIT_ASSERT_EQUAL(2 * planning_horizon + 1, (uint32_t) vec.size());
			}
			
			void testValidSlot() {
				// Even though this class is a friend class of ReservationTable, my compiler won't allow me to access protected functions.
				// Hence I will test ReservationTable::isValid(...) through the mark function.
				Reservation reservation = Reservation(MacId(1), Reservation::Action::BUSY);
				
				// Entire planning horizon, both negative and positive, should be valid.
				for (int32_t offset = -int32_t(this->planning_horizon);
				     offset <= int32_t(this->planning_horizon); offset++) {
					bool exception_thrown = false;
					try {
						table->mark(offset, reservation);
					} catch (const std::exception& e) {
						exception_thrown = true;
					}
					CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
				}
				
				// Going beyond the planning horizon in either direction should be invalid.
				int32_t move_into_invalid_range = 10;
				for (int32_t offset = -int32_t(this->planning_horizon - move_into_invalid_range);
				     offset < -int32_t(this->planning_horizon); offset++) {
					bool exception_thrown = false;
					try {
						table->mark(offset, reservation);
					} catch (const std::exception& e) {
						exception_thrown = true;
					}
					CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
				}
				for (int32_t offset = int32_t(this->planning_horizon + move_into_invalid_range);
				     offset > int32_t(this->planning_horizon); offset--) {
					bool exception_thrown = false;
					try {
						table->mark(offset, reservation);
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
				for (int32_t range_start = -int32_t(this->planning_horizon);
				     range_start <= int32_t(range_length); range_start++) {
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
				for (int32_t range_start = 0;
				     range_start <= int32_t(this->planning_horizon) - int32_t(range_length); range_start++) {
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
					table->isUtilized(-int32_t(planning_horizon) - 1, range_length);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
				
				// Going outside the allowed positive range should be invalid.
				exception_thrown = false;
				try {
					table->isUtilized(planning_horizon, range_length);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			}
			
			void testMarking() {
				Reservation busy_reservation = Reservation(MacId(1), Reservation::Action::BUSY);
				Reservation idle_reservation = Reservation(MacId(1), Reservation::Action::IDLE);
				int32_t slot_offset = 0;
				CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(slot_offset));
				table->mark(slot_offset, busy_reservation);
				CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(slot_offset));
				table->mark(slot_offset, idle_reservation);
				CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(slot_offset));
				slot_offset = -1;
				table->mark(slot_offset, busy_reservation);
				CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(slot_offset));
				CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(slot_offset + 1));
				CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(slot_offset + 2));
			}
			
			void testIdleRange() {
				Reservation reservation = Reservation(MacId(1), Reservation::Action::BUSY);
				uint32_t length = 2;
				// Negative range.
				for (int32_t start_range = -int32_t(planning_horizon); start_range < -length; start_range++) {
					// Everything should be idle so far.
					CPPUNIT_ASSERT_EQUAL(true, table->isIdle(start_range, length));
					// Mark every second starting point as utilized for the second for-loop.
					if (start_range % 2 == 0)
						table->mark(start_range, reservation);
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
						table->mark(start_range, reservation);
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
						table->mark(start_range, reservation);
				}
				// Everything so far should now be regarded as utilized.
				for (int32_t start_range = 1; start_range < this->planning_horizon - int32_t(length); start_range++) {
					CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(start_range, length));
					if (start_range % 2 == 0)
						CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(start_range));
				}
			}
			
			void testFindIdleRange() {
				Reservation reservation = Reservation(MacId(1), Reservation::Action::BUSY);
				int32_t start = int32_t(planning_horizon) - 2;
				// start + length exceeds planning horizon ...
				uint32_t length = 4;
				bool exception_thrown = false;
				try {
					table->findEarliestIdleRange(start, length, false);
				} catch (const std::invalid_argument& e) {
					exception_thrown = true;
				}
				// ... so an exception should be thrown.
				CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
				
				// Now we can stay 'just' within the valid range.
				length = 3;
				exception_thrown = false;
				try {
					int32_t start_of_idle_range = table->findEarliestIdleRange(start, length, false);
					CPPUNIT_ASSERT_EQUAL(start, start_of_idle_range);
				} catch (const std::exception& e) {
					exception_thrown = true;
					std::cout << e.what() << std::endl;
				}
				CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
				
				// Starting at 'now' works too.
				start = 0;
				int32_t start_of_idle_range = table->findEarliestIdleRange(start, length, false);
				CPPUNIT_ASSERT_EQUAL(start, start_of_idle_range);
				
				// But starting in the p
				int32_t idle_slot_range_start = table->findEarliestIdleRange(0, 5, false);
				CPPUNIT_ASSERT_EQUAL(int32_t(0), idle_slot_range_start);
				table->mark(0, reservation);
				// x00000
				// 012345
				idle_slot_range_start = table->findEarliestIdleRange(0, 5, false);
				CPPUNIT_ASSERT_EQUAL(int32_t(1), idle_slot_range_start);
				table->mark(5, reservation);
				// x0000x0000
				// 0123456789
				idle_slot_range_start = table->findEarliestIdleRange(0, 5, false);
				CPPUNIT_ASSERT_EQUAL(int32_t(6), idle_slot_range_start);
				table->mark(11, reservation);
				// x0000x00000x0
				// 0123456789012
				idle_slot_range_start = table->findEarliestIdleRange(0, 5, false);
				CPPUNIT_ASSERT_EQUAL(int32_t(6), idle_slot_range_start);
				idle_slot_range_start = table->findEarliestIdleRange(7, 5, false);
				CPPUNIT_ASSERT_EQUAL(int32_t(12), idle_slot_range_start);
			}
			
			void testUpdate() {
				Reservation reservation = Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::BUSY);
				CPPUNIT_ASSERT_EQUAL(true, table->isIdle(0, planning_horizon));
				table->mark(planning_horizon, reservation);
				CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(0, planning_horizon + 1));
				CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(planning_horizon));
				table->update(1);
				for (int32_t offset = 0; offset <= int32_t(planning_horizon); offset++) {
					if (offset == int32_t(planning_horizon) - 1)
						CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(offset));
					else
						CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(offset));
				}
				
				table->update(2);
				for (int32_t offset = 0; offset <= int32_t(planning_horizon); offset++) {
					if (offset == int32_t(planning_horizon) - (1 + 2))
						CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(offset));
					else
						CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(offset));
				}
				
				table->update(7);
				for (int32_t offset = 0; offset <= int32_t(planning_horizon); offset++) {
					if (offset == int32_t(planning_horizon) - (1 + 2 + 7))
						CPPUNIT_ASSERT_EQUAL(true, table->isUtilized(offset));
					else
						CPPUNIT_ASSERT_EQUAL(false, table->isUtilized(offset));
				}
			}
			
			void testLastUpdated() {
				Timestamp now = Timestamp();
				CPPUNIT_ASSERT_EQUAL(true, table->getCurrentSlot() == now);
				now += 1;
				CPPUNIT_ASSERT_EQUAL(false, table->getCurrentSlot() == now);
				table->update(1);
				CPPUNIT_ASSERT_EQUAL(true, table->getCurrentSlot() == now);
				table->update(13);
				CPPUNIT_ASSERT_EQUAL(false, table->getCurrentSlot() == now);
				now += 13;
				CPPUNIT_ASSERT_EQUAL(true, table->getCurrentSlot() == now);
			}
			
			void testNumIdleSlots() {
				// At first, the entire future planning horizon + the current slot should be idle.
				CPPUNIT_ASSERT_EQUAL(uint64_t(planning_horizon + 1), table->getNumIdleSlots());
				// If we mark something as idle now, it shouldn't change the number of idle slots.
				table->mark(0, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::IDLE));
				CPPUNIT_ASSERT_EQUAL(uint64_t(planning_horizon + 1), table->getNumIdleSlots());
				// Marking something as busy *should* change the number.
				table->mark(0, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::BUSY));
				CPPUNIT_ASSERT_EQUAL(uint64_t(planning_horizon), table->getNumIdleSlots());
				// Now revert again.
				table->mark(0, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::IDLE));
				CPPUNIT_ASSERT_EQUAL(uint64_t(planning_horizon + 1), table->getNumIdleSlots());
				for (uint32_t i = 0; i < planning_horizon; i++) {
					table->mark(i, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::BUSY));
					CPPUNIT_ASSERT_EQUAL(uint64_t(planning_horizon + 1 - (i+1)), table->getNumIdleSlots());
				}
			}
			
			void testFindCandidateSlotsAllIdle() {
				// This test requires that the planning horizon is 25.
				CPPUNIT_ASSERT_EQUAL(uint32_t(25), planning_horizon);
				unsigned int min_offset = 0, num_candidates = 5, range_length = 5;
				// At first, all slots are free.
				std::vector<int32_t> candidate_slots = table->findCandidateSlots(min_offset, num_candidates, range_length, false);
				// So we should have no problem finding enough candidates.
				CPPUNIT_ASSERT_EQUAL(size_t(num_candidates), candidate_slots.size());
				// And these should be consecutive slots starting at 0.
				for (int32_t i = 0; i < num_candidates; i++)
					CPPUNIT_ASSERT_EQUAL(candidate_slots.at(i), int32_t(min_offset + i));
			}
			
			void testFindCandidateSlotsComplicated() {
				// This test requires that the planning horizon is 25.
				CPPUNIT_ASSERT_EQUAL(uint32_t(25), planning_horizon);
				unsigned int min_offset = 0, num_candidates = 5, range_length = 5;
				// Mark some slots as busy...
				table->mark(0, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::BUSY));
				table->mark(6, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::BUSY));
				table->mark(13, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::BUSY));
				table->mark(19, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::BUSY));
				table->mark(20, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::BUSY));
				table->mark(25, Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::BUSY));
				std::vector<int32_t> candidate_slots = table->findCandidateSlots(min_offset, num_candidates, range_length, false);
				// We should only be able to find 4 candidates.
				CPPUNIT_ASSERT_EQUAL(size_t(4), candidate_slots.size());
				// And these should be the following starting slots:
				CPPUNIT_ASSERT_EQUAL(int32_t(1), candidate_slots.at(0));
				CPPUNIT_ASSERT_EQUAL(int32_t(7), candidate_slots.at(1));
				CPPUNIT_ASSERT_EQUAL(int32_t(8), candidate_slots.at(2));
				CPPUNIT_ASSERT_EQUAL(int32_t(14), candidate_slots.at(3));
			}
			
			void testFindEarliestOffset() {
				int32_t offset1 = 10, offset2 = offset1 + 2, offset3 = offset2 + 1;
				CPPUNIT_ASSERT(offset1 < planning_horizon); // otherwise this test won't work
				CPPUNIT_ASSERT(offset2 < planning_horizon); // otherwise this test won't work
				Reservation reservation = Reservation(MacId(0), Reservation::Action::TX);
				table->mark(offset1, reservation);
				table->mark(offset2, reservation);
				CPPUNIT_ASSERT_EQUAL(offset1, table->findEarliestOffset(int32_t(0), reservation));
				CPPUNIT_ASSERT_EQUAL(offset2, table->findEarliestOffset(int32_t(offset1 + 1), reservation));
				
				bool exception_occurred = false;
				try {
					table->findEarliestOffset(int32_t(offset3), reservation);
				} catch (const std::runtime_error& e) {
					exception_occurred = true;
				}
				CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
				
				exception_occurred = false;
				reservation.setAction(Reservation::Action::RX);
				try {
					table->findEarliestOffset(int32_t(offset3), reservation);
				} catch (const std::runtime_error& e) {
					exception_occurred = true;
				}
				CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
			}
			
			void testMultiSlotTransmissionReservationTx() {
				uint32_t num_cont_slots = 4; // 5 slots in total
				Reservation reservation = Reservation(MacId(42), Reservation::Action::TX, num_cont_slots);
				table->mark(0, reservation);
				for (uint32_t t = 0; t < planning_horizon; t++) {
					if (t == 0)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isTx());
					else if (t <= num_cont_slots)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isTxCont());
					else
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isIdle());
				}
			}
			
			void testMultiSlotTransmissionReservationRx() {
				uint32_t num_cont_slots = 4; // 5 slots in total
				Reservation reservation = Reservation(MacId(42), Reservation::Action::RX, num_cont_slots);
				table->mark(0, reservation);
				for (uint32_t t = 0; t < planning_horizon; t++) {
					if (t == 0)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isRx());
					else if (t <= num_cont_slots)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isRx());
					else
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isIdle());
				}
			}
			
			void testMultiSlotTransmissionReservationBusy() {
				uint32_t num_cont_slots = 4; // 5 slots in total
				Reservation reservation = Reservation(MacId(42), Reservation::Action::BUSY, num_cont_slots);
				table->mark(0, reservation);
				for (uint32_t t = 0; t < planning_horizon; t++) {
					if (t == 0)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isBusy());
					else if (t <= num_cont_slots)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isBusy());
					else
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isIdle());
				}
			}
			
			void testMultiSlotTransmissionReservationIdle() {
				uint32_t num_cont_slots = 4; // 5 slots in total
				Reservation reservation = Reservation(MacId(42), Reservation::Action::IDLE, num_cont_slots);
				table->mark(0, reservation);
				for (uint32_t t = 0; t < planning_horizon; t++) {
					if (t == 0)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isIdle());
					else if (t <= num_cont_slots)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isIdle());
					else
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isIdle());
				}
			}
			
			void testMultiSlotTransmissionReservationTxCont() {
				uint32_t num_cont_slots = 4; // 5 slots in total
				Reservation reservation = Reservation(MacId(42), Reservation::Action::TX_CONT, num_cont_slots);
				table->mark(0, reservation);
				for (uint32_t t = 0; t < planning_horizon; t++) {
					if (t == 0)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isTxCont());
					else if (t <= num_cont_slots)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isTxCont());
					else
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isIdle());
				}
			}
			
			void testCountReservedTxSlots() {
				MacId id = MacId(42);
				CPPUNIT_ASSERT_EQUAL(ulong(0), table->countReservedTxSlots(id));
				unsigned long marked = 7;
				for (unsigned long i = 0; i < marked; i++)
					table->mark(i, Reservation(id, Reservation::TX));
				CPPUNIT_ASSERT_EQUAL(marked, table->countReservedTxSlots(id));
				table->mark(marked, Reservation(id, Reservation::IDLE)); // doesn't count
				CPPUNIT_ASSERT_EQUAL(marked, table->countReservedTxSlots(id));
				table->mark(marked + 1, Reservation(id, Reservation::BUSY)); // doesn't count
				CPPUNIT_ASSERT_EQUAL(marked, table->countReservedTxSlots(id));
				table->mark(marked + 2, Reservation(id, Reservation::TX_CONT)); // *does* count
				CPPUNIT_ASSERT_EQUAL(marked + 1, table->countReservedTxSlots(id));
				// Another user's reservations shouldn't be counted.
				MacId other_id = MacId(id.getId() + 1);
				table->mark(marked + 3, Reservation(other_id, Reservation::TX));
				table->mark(marked + 4, Reservation(other_id, Reservation::TX_CONT));
				table->mark(marked + 5, Reservation(other_id, Reservation::IDLE));
				table->mark(marked + 6, Reservation(other_id, Reservation::BUSY));
				CPPUNIT_ASSERT_EQUAL(marked + 1, table->countReservedTxSlots(id));
			}
			
			void testGetTxReservations() {
				MacId id1 = MacId(42), id2 = MacId(43);
				for (int i = 3; i < 7; i++)
					table->mark(i, Reservation(id1, Reservation::Action::TX));
				for (int i = 12; i < 22; i++)
					table->mark(i, Reservation(id2, Reservation::Action::TX));
				table->mark(0, Reservation(id1, Reservation::BUSY));
				table->mark(1, Reservation(id2, Reservation::BUSY));
				ReservationTable* tbl1 = table->getTxReservations(id1);
				for (int i = 0; i < planning_horizon; i++) {
					if (i >= 3 && i < 7)
						CPPUNIT_ASSERT_EQUAL(id1,
						                     tbl1->slot_utilization_vec.at(tbl1->convertOffsetToIndex(i)).getTarget());
					else
						CPPUNIT_ASSERT_EQUAL(SYMBOLIC_ID_UNSET,
						                     tbl1->slot_utilization_vec.at(tbl1->convertOffsetToIndex(i)).getTarget());
				}
				
				ReservationTable* tbl2 = table->getTxReservations(id2);
				for (int i = 0; i < planning_horizon; i++) {
					if (i >= 12 && i < 22)
						CPPUNIT_ASSERT_EQUAL(id2,
						                     tbl2->slot_utilization_vec.at(tbl1->convertOffsetToIndex(i)).getTarget());
					else
						CPPUNIT_ASSERT_EQUAL(SYMBOLIC_ID_UNSET,
						                     tbl2->slot_utilization_vec.at(tbl1->convertOffsetToIndex(i)).getTarget());
				}
				
				delete tbl1;
				delete tbl2;
			}
			
			void testIntegrateTxReservations() {
				int32_t offset = 5;
				Reservation reservation = Reservation(MacId(42), Reservation::Action::TX);
				CPPUNIT_ASSERT(table->getReservation(offset) == Reservation());
				table->mark(offset, reservation);
				CPPUNIT_ASSERT(table->getReservation(offset) == reservation);
				ReservationTable other = ReservationTable(planning_horizon);
				CPPUNIT_ASSERT(other.getReservation(offset) == Reservation());
				other.integrateTxReservations(table);
				CPPUNIT_ASSERT(other.getReservation(offset) == reservation);
			}
			
			void testAnyTxReservations() {
				MacId id = MacId(42);
				int32_t offset = 5;
				Reservation reservation = Reservation(id, Reservation::Action::TX);
				CPPUNIT_ASSERT_EQUAL(false, table->anyTxReservations(0, 2*offset));
				table->mark(offset, reservation);
				CPPUNIT_ASSERT_EQUAL(true, table->anyTxReservations(0, 2*offset));
				table->mark(offset, Reservation(id, Reservation::IDLE));
				CPPUNIT_ASSERT_EQUAL(false, table->anyTxReservations(0, 2*offset));
				table->mark(offset, Reservation(id, Reservation::BUSY));
				CPPUNIT_ASSERT_EQUAL(false, table->anyTxReservations(0, 2*offset));
				table->mark(offset, Reservation(id, Reservation::TX_CONT));
				CPPUNIT_ASSERT_EQUAL(true, table->anyTxReservations(0, 2*offset));
			}

            void testAnyRxReservations() {
                MacId id = MacId(42);
                int32_t offset = 5;
                Reservation reservation = Reservation(id, Reservation::Action::RX);
                CPPUNIT_ASSERT_EQUAL(false, table->anyRxReservations(0, 2*offset));
                table->mark(offset, reservation);
                CPPUNIT_ASSERT_EQUAL(true, table->anyRxReservations(0, 2*offset));
                table->mark(offset, Reservation(id, Reservation::IDLE));
                CPPUNIT_ASSERT_EQUAL(false, table->anyRxReservations(0, 2*offset));
                table->mark(offset, Reservation(id, Reservation::BUSY));
                CPPUNIT_ASSERT_EQUAL(false, table->anyRxReservations(0, 2*offset));
                table->mark(offset, Reservation(id, Reservation::RX));
                CPPUNIT_ASSERT_EQUAL(true, table->anyRxReservations(0, 2*offset));
            }
			
			void testLocking() {
				// Find some candidate
				unsigned int num_candidates = 3;
				std::vector<int32_t> slots = table->findCandidateSlots(0, num_candidates, 5, false);
				// Now lock these slots.
				table->lock(slots);
				// So these slots should *not* be considered for a further request.
				std::vector<int32_t> slots2 = table->findCandidateSlots(0, num_candidates, 5, false);
				CPPUNIT_ASSERT_EQUAL(slots.size(), slots2.size());
				
				for (int32_t i : slots) { // for every slot out of the first set
					for (int32_t j : slots2) // it shouldn't equal any slot out of the second
						CPPUNIT_ASSERT(i != j);
				}
				
				// First slots should be 0, 1, 2
				for (int32_t i = 0; i < num_candidates; i++)
					CPPUNIT_ASSERT_EQUAL(i, slots.at(i));
				// Second slots should be 3, 4, 5
				for (int32_t i = 0; i < num_candidates; i++)
					CPPUNIT_ASSERT_EQUAL(i+(int32_t) num_candidates, slots2.at(i));
			}
		
		CPPUNIT_TEST_SUITE(ReservationTableTests);
			CPPUNIT_TEST(testConstructor);
			CPPUNIT_TEST(testPlanningHorizon);
			CPPUNIT_TEST(testValidSlot);
			CPPUNIT_TEST(testValidSlotRange);
			CPPUNIT_TEST(testMarking);
			CPPUNIT_TEST(testIdleRange);
			CPPUNIT_TEST(testFindIdleRange);
			CPPUNIT_TEST(testUpdate);
			CPPUNIT_TEST(testLastUpdated);
			CPPUNIT_TEST(testNumIdleSlots);
			CPPUNIT_TEST(testFindCandidateSlotsAllIdle);
			CPPUNIT_TEST(testFindCandidateSlotsComplicated);
			CPPUNIT_TEST(testFindEarliestOffset);
			CPPUNIT_TEST(testMultiSlotTransmissionReservationTx);
			CPPUNIT_TEST(testMultiSlotTransmissionReservationRx);
			CPPUNIT_TEST(testMultiSlotTransmissionReservationBusy);
			CPPUNIT_TEST(testMultiSlotTransmissionReservationIdle);
			CPPUNIT_TEST(testMultiSlotTransmissionReservationTxCont);
			CPPUNIT_TEST(testCountReservedTxSlots);
			CPPUNIT_TEST(testGetTxReservations);
			CPPUNIT_TEST(testIntegrateTxReservations);
			CPPUNIT_TEST(testAnyTxReservations);
            CPPUNIT_TEST(testAnyRxReservations);
			CPPUNIT_TEST(testLocking);
		CPPUNIT_TEST_SUITE_END();
	};
	
}