// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ReservationTable.hpp"
#include "../coutdebug.hpp"
#include <algorithm>

namespace TUHH_INTAIRNET_MCSOTDMA {

	class ReservationTableTests : public CppUnit::TestFixture {
	private:
		uint32_t planning_horizon;
		ReservationTable *table, *table_rx_1, *table_rx_2, *table_tx;

	public:
		void setUp() override {
			planning_horizon = 25;
			table = new ReservationTable(planning_horizon);
			table_rx_1 = new ReservationTable(planning_horizon);
			table_rx_2 = new ReservationTable(planning_horizon);
			table_tx = new ReservationTable(planning_horizon);
			table->linkTransmitterReservationTable(table_tx);
			table->linkReceiverReservationTable(table_rx_1);
			table->linkReceiverReservationTable(table_rx_2);
		}

		void tearDown() override {
			delete table;
			delete table_rx_1;
			delete table_rx_2;
			delete table_tx;
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
			for (auto offset = int32_t(this->planning_horizon + move_into_invalid_range);
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
				CPPUNIT_ASSERT_EQUAL(uint64_t(planning_horizon + 1 - (i + 1)), table->getNumIdleSlots());
			}
		}

		void testFindCandidateSlotsAllIdle() {
			// This test requires that the planning horizon is 25.
			CPPUNIT_ASSERT_EQUAL(uint32_t(25), planning_horizon);
			unsigned int min_offset = 0, num_candidates = 5;
			int num_bursts_forward = 1, num_bursts_reverse = 1, period = 0, timeout = 2;
			// At first, all slots are free.
			std::vector<unsigned int> candidate_slots = table->findPPCandidates(num_candidates, min_offset, num_bursts_forward, num_bursts_reverse, period, timeout);
			// So we should have no problem finding enough candidates.
			CPPUNIT_ASSERT_EQUAL(size_t(num_candidates), candidate_slots.size());
			// And these should be consecutive slots starting at 0.
			for (int32_t i = 0; i < num_candidates; i++)
				CPPUNIT_ASSERT_EQUAL(candidate_slots.at(i), uint32_t(min_offset + i));
		}

		// void testFindCandidateSlotsRespectRX() {			
		// 	CPPUNIT_ASSERT_EQUAL(uint32_t(25), planning_horizon);
		// 	unsigned int min_offset = 0, num_candidates = 3, burst_offset = 5, burst_length = 2, burst_length_tx = 0, timeout = 1;
		// 	// At first, all slots are free.
		// 	std::vector<unsigned int> candidate_slots = table->findPPCandidates(num_candidates, min_offset, burst_offset, burst_length, burst_length_tx, timeout, nullptr);
		// 	// So we should have no problem finding enough candidates.
		// 	CPPUNIT_ASSERT_EQUAL(size_t(num_candidates), candidate_slots.size());
		// 	// And these should be consecutive slots starting at 0.
		// 	for (int32_t i = 0; i < num_candidates; i++)
		// 		CPPUNIT_ASSERT_EQUAL(uint32_t(min_offset + i), candidate_slots.at(i));

		// 	// Now mark some slots as busy for the receiver.
		// 	table_rx_1->lock(0, MacId(42));
		// 	table_rx_2->lock(0, MacId(42));
		// 	// CPPUNIT_ASSERT_EQUAL(false, table->isBurstValid(0, burst_length, burst_length_tx, false));
		// 	candidate_slots = table->findPPCandidates(num_candidates, min_offset, burst_offset, burst_length, burst_length_tx, timeout, nullptr);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(num_candidates), candidate_slots.size());
		// 	// And these should be consecutive slots starting at 1.
		// 	for (int32_t i = 0; i < num_candidates; i++) 				
		// 		CPPUNIT_ASSERT_EQUAL(uint32_t(min_offset + i + 1), candidate_slots.at(i));			

		// 	table_rx_1->lock(2, MacId(42));
		// 	table_rx_2->lock(2, MacId(42));
		// 	burst_length = 1;
		// 	candidate_slots = table->findPPCandidates(num_candidates, min_offset, burst_offset, burst_length, burst_length_tx, timeout, nullptr);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(num_candidates), candidate_slots.size());
		// 	// And these should be consecutive slots starting at 1 and exclude 2.
		// 	std::vector<int> expected_slots = {1, 3, 4, 5, 6};
		// 	for (int32_t i = 0, j = 0; i < num_candidates; i++)
		// 		CPPUNIT_ASSERT(std::find(expected_slots.begin(), expected_slots.end(), 5) != expected_slots.end());
		// }

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

		void testCountReservedTxSlots() {
			MacId id = MacId(42);
			CPPUNIT_ASSERT_EQUAL((unsigned long)(0), table->countReservedTxSlots(id));
			unsigned long marked = 7;
			for (unsigned long i = 0; i < marked; i++)
				table->mark(i, Reservation(id, Reservation::TX));
			CPPUNIT_ASSERT_EQUAL(marked, table->countReservedTxSlots(id));
			table->mark(marked, Reservation(id, Reservation::IDLE)); // doesn't count
			CPPUNIT_ASSERT_EQUAL(marked, table->countReservedTxSlots(id));
			table->mark(marked + 1, Reservation(id, Reservation::BUSY)); // doesn't count			
			CPPUNIT_ASSERT_EQUAL(marked, table->countReservedTxSlots(id));
			// Another user's reservations shouldn't be counted.
			MacId other_id = MacId(id.getId() + 1);
			table->mark(marked + 3, Reservation(other_id, Reservation::TX));
			table->mark(marked + 5, Reservation(other_id, Reservation::IDLE));
			table->mark(marked + 6, Reservation(other_id, Reservation::BUSY));
			CPPUNIT_ASSERT_EQUAL(marked, table->countReservedTxSlots(id));
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
			table->transmitter_reservation_table = nullptr;
			MacId id = MacId(42);
			int32_t offset = 5;
			Reservation reservation = Reservation(id, Reservation::Action::TX);
			CPPUNIT_ASSERT_EQUAL(false, table->anyTxReservations(0, 2 * offset));
			table->mark(offset, reservation);
			CPPUNIT_ASSERT_EQUAL(true, table->anyTxReservations(0, 2 * offset));
			table->mark(offset, Reservation(id, Reservation::IDLE));
			CPPUNIT_ASSERT_EQUAL(false, table->anyTxReservations(0, 2 * offset));
			table->mark(offset, Reservation(id, Reservation::BUSY));
			CPPUNIT_ASSERT_EQUAL(false, table->anyTxReservations(0, 2 * offset));			
		}

		void testAnyRxReservations() {
			MacId id = MacId(42);
			int32_t offset = 5;
			Reservation reservation = Reservation(id, Reservation::Action::RX);
			CPPUNIT_ASSERT_EQUAL(false, table->anyRxReservations(0, 2 * offset));
			table->mark(offset, reservation);
			CPPUNIT_ASSERT_EQUAL(true, table->anyRxReservations(0, 2 * offset));
			table->mark(offset, Reservation(id, Reservation::IDLE));
			CPPUNIT_ASSERT_EQUAL(false, table->anyRxReservations(0, 2 * offset));
			table->mark(offset, Reservation(id, Reservation::BUSY));
			CPPUNIT_ASSERT_EQUAL(false, table->anyRxReservations(0, 2 * offset));
			table->mark(offset, Reservation(id, Reservation::RX));
			CPPUNIT_ASSERT_EQUAL(true, table->anyRxReservations(0, 2 * offset));
		}

		// void testLocking() {
		// 	// Find some candidate
		// 	unsigned int num_candidates = 3;
		// 	std::vector<unsigned int> slots = table->findPPCandidates(num_candidates, 0, 0, 5, 5, 1, nullptr);
		// 	// Now lock these slots.
		// 	for (auto t : slots)
		// 		table->lock(t, MacId(42));
		// 	// So these slots should *not* be considered for a further request.
		// 	std::vector<unsigned int> slots2 = table->findPPCandidates(num_candidates, 0, 0, 5, 5, 1, nullptr);
		// 	CPPUNIT_ASSERT_EQUAL(slots.size(), slots2.size());

		// 	for (int32_t i : slots) { // for every slot out of the first set
		// 		for (int32_t j : slots2) // it shouldn't equal any slot out of the second
		// 			CPPUNIT_ASSERT(i != j);
		// 	}

		// 	// First slots should be 0, 1, 2
		// 	for (unsigned int i = 0; i < num_candidates; i++)
		// 		CPPUNIT_ASSERT_EQUAL(i, slots.at(i));
		// 	// Second slots should be 3, 4, 5
		// 	for (unsigned int i = 0; i < num_candidates; i++)
		// 		CPPUNIT_ASSERT_EQUAL(i + (int32_t) num_candidates, slots2.at(i));
		// }

		void testLinkedTXTable() {
//			ReservationTable tx_table = ReservationTable(planning_horizon);
//			table->linkTransmitterReservationTable(&tx_table);

			table->mark(0, Reservation(SYMBOLIC_ID_UNSET, Reservation::TX));
			CPPUNIT_ASSERT_EQUAL(Reservation::TX, table->getReservation(0).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::TX, table_tx->getReservation(0).getAction());			

			table->mark(2, Reservation(SYMBOLIC_ID_UNSET, Reservation::RX));
			CPPUNIT_ASSERT_EQUAL(Reservation::TX, table->getReservation(0).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::TX, table_tx->getReservation(0).getAction());			
			// RX in own table, but RX does *not* forward to a transmitter table.
			CPPUNIT_ASSERT_EQUAL(Reservation::RX, table->getReservation(2).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::IDLE, table_tx->getReservation(2).getAction());

			// A 2nd TX reservation should throw an exception because we support only one transmitter.
			ReservationTable second_table = ReservationTable(planning_horizon);
			second_table.linkTransmitterReservationTable(table_tx);
			CPPUNIT_ASSERT_THROW(second_table.mark(0, Reservation(SYMBOLIC_ID_UNSET, Reservation::TX)), no_tx_available_error);
			CPPUNIT_ASSERT_THROW(second_table.mark(0, Reservation(MacId(1), Reservation::TX)), no_tx_available_error);

			table->lock(3, MacId(42));
			table_tx->lock(3, MacId(42));
			CPPUNIT_ASSERT_EQUAL(Reservation::LOCKED, table->getReservation(3).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::LOCKED, table_tx->getReservation(3).getAction());
		}

		void testLinkedRXTables() {
			// RX reservation should forward to *one* receiver table.
			table->mark(0, Reservation(SYMBOLIC_ID_UNSET, Reservation::RX));
			CPPUNIT_ASSERT_EQUAL(Reservation::RX, table->getReservation(0).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::RX, table_rx_1->getReservation(0).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::IDLE, table_rx_2->getReservation(0).getAction());

			// TX reservations shouldn't.
			table->mark(1, Reservation(SYMBOLIC_ID_UNSET, Reservation::TX));
			CPPUNIT_ASSERT_EQUAL(Reservation::TX, table->getReservation(1).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::IDLE, table_rx_1->getReservation(1).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::IDLE, table_rx_2->getReservation(1).getAction());

			// a 2nd RX reservation should forward to the *other* receiver table.
			ReservationTable second_table = ReservationTable(planning_horizon);
			second_table.linkReceiverReservationTable(table_rx_1);
			second_table.linkReceiverReservationTable(table_rx_2);
			second_table.mark(0, Reservation(SYMBOLIC_ID_UNSET, Reservation::RX));
			CPPUNIT_ASSERT_EQUAL(Reservation::RX, table->getReservation(0).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::RX, second_table.getReservation(0).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::RX, table_rx_1->getReservation(0).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::RX, table_rx_2->getReservation(0).getAction());

			// a 3rd RX reservation should throw an exception because we only have two linked receiver tables and both are not idle
			ReservationTable third_table = ReservationTable(planning_horizon);
			third_table.linkReceiverReservationTable(table_rx_1);
			third_table.linkReceiverReservationTable(table_rx_2);
			CPPUNIT_ASSERT_THROW(third_table.mark(0, Reservation(SYMBOLIC_ID_UNSET, Reservation::RX)), no_rx_available_error);
			CPPUNIT_ASSERT_THROW(third_table.mark(0, Reservation(MacId(1), Reservation::RX)), no_rx_available_error);
		}

		void testDefaultReservation() {
			ReservationTable bc_table = ReservationTable(planning_horizon, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::RX));
			for (const auto& reservation : bc_table.slot_utilization_vec) {
				CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, reservation.getTarget());
				CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, reservation.getAction());
			}
		}

		// void testFindEarliestIdleSlots() {
		// 	unsigned int min_offset = 0, burst_length = 5, burst_length_tx = 3;
		// 	unsigned int burst_offset = 7, timeout = 2;
		// 	unsigned int start_slot = table->findEarliestIdleSlotsPP(min_offset, burst_length, burst_length_tx, burst_offset, timeout, nullptr);
		// 	CPPUNIT_ASSERT_EQUAL(uint32_t(0), start_slot);

		// 	const Reservation res = Reservation(MacId(5), Reservation::BUSY);
		// 	table_rx_1->mark(burst_length_tx, res);
		// 	start_slot = table->findEarliestIdleSlotsPP(min_offset, burst_length, burst_length_tx, burst_offset, timeout, nullptr);
		// 	CPPUNIT_ASSERT_EQUAL(uint32_t(0), start_slot);

		// 	table_rx_2->mark(burst_length_tx, res);
		// 	start_slot = table->findEarliestIdleSlotsPP(min_offset, burst_length, burst_length_tx, burst_offset, timeout, nullptr);
		// 	CPPUNIT_ASSERT_EQUAL(uint32_t(1), start_slot);

		// 	table_tx->mark(min_offset + 1, res);			
		// 	start_slot = table->findEarliestIdleSlotsPP(min_offset, burst_length, burst_length_tx, burst_offset, timeout, nullptr);
		// 	CPPUNIT_ASSERT_EQUAL(uint32_t(2), start_slot);

		// 	table_rx_1->mark(min_offset + burst_offset + burst_length_tx + 2, res);
		// 	table_rx_2->mark(min_offset + burst_offset + burst_length_tx + 2, res);
		// 	start_slot = table->findEarliestIdleSlotsPP(min_offset, burst_length, burst_length_tx, burst_offset, timeout, nullptr);
		// 	CPPUNIT_ASSERT_EQUAL(uint32_t(3), start_slot);

		// 	table_rx_1->mark(burst_offset + 3 + burst_length_tx, res);
		// 	table_rx_2->mark(burst_offset + 3 + burst_length_tx, res);
		// 	start_slot = table->findEarliestIdleSlotsPP(min_offset, burst_length, burst_length_tx, burst_offset, timeout, nullptr);
		// 	CPPUNIT_ASSERT_EQUAL(uint32_t(4), start_slot);
		// }

	CPPUNIT_TEST_SUITE(ReservationTableTests);
			CPPUNIT_TEST(testConstructor);
			CPPUNIT_TEST(testPlanningHorizon);
			CPPUNIT_TEST(testValidSlot);
			CPPUNIT_TEST(testValidSlotRange);
			CPPUNIT_TEST(testMarking);
			CPPUNIT_TEST(testIdleRange);
			CPPUNIT_TEST(testUpdate);
			CPPUNIT_TEST(testLastUpdated);
			CPPUNIT_TEST(testNumIdleSlots);
			CPPUNIT_TEST(testFindCandidateSlotsAllIdle);
			CPPUNIT_TEST(testFindEarliestOffset);
			// CPPUNIT_TEST(testFindCandidateSlotsRespectRX);			
			CPPUNIT_TEST(testCountReservedTxSlots);
			CPPUNIT_TEST(testGetTxReservations);
			CPPUNIT_TEST(testIntegrateTxReservations);
			CPPUNIT_TEST(testAnyTxReservations);
			CPPUNIT_TEST(testAnyRxReservations);
			// CPPUNIT_TEST(testLocking);
			CPPUNIT_TEST(testLinkedTXTable);
			CPPUNIT_TEST(testLinkedRXTables);			
			CPPUNIT_TEST(testDefaultReservation);
			// CPPUNIT_TEST(testFindEarliestIdleSlots);
		CPPUNIT_TEST_SUITE_END();
	};

}