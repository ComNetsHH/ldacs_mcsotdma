//
// Created by Sebastian Lindner on 14.10.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ReservationTable.hpp"
#include "../ReservationManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

class ReservationManagerTests : public CppUnit::TestFixture {
	private:
		ReservationManager* reservation_manager;
	
	public:
		void setUp() override {
			uint32_t planning_horizon = 1024;
            reservation_manager = new ReservationManager(planning_horizon);
		}
		
		void tearDown() override {
			delete reservation_manager;
		}
		
		void testAddFreqChannel() {
			bool p2p_channel = true;
			uint64_t center_freq = 1000;
			uint64_t bandwidth = 500;
			
			// Fetching it now should throw an exception.
			bool exception_thrown = false;
			try {
				const FrequencyChannel* channel = reservation_manager->getFreqChannel(0);
			} catch(const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			
			// Add it.
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq, bandwidth);
			
			// Now we should be able to get both reservation table and frequency channel.
			exception_thrown = false;
			try {
				const FrequencyChannel* channel = reservation_manager->getFreqChannel(0);
				const ReservationTable* table = reservation_manager->getReservationTable(0);
			} catch(const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
		}
		
		void testUpdate() {
			bool p2p_channel = true;
			uint64_t center_freq1 = 1000, center_freq2 = center_freq1 + 1;
			uint64_t bandwidth = 500;
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq1, bandwidth);
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq2, bandwidth);
			Timestamp now = Timestamp();
			CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTable(0)->getCurrentSlot() == now);
			CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTable(1)->getCurrentSlot() == now);
			
			uint64_t num_slots = 5;
			reservation_manager->update(num_slots);
			CPPUNIT_ASSERT_EQUAL(false, reservation_manager->getReservationTable(0)->getCurrentSlot() == now);
			CPPUNIT_ASSERT_EQUAL(false, reservation_manager->getReservationTable(1)->getCurrentSlot() == now);
			
			now += num_slots;
			
			CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTable(0)->getCurrentSlot() == now);
			CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTable(1)->getCurrentSlot() == now);
		}

		void testGetLeastUtilizedReservationTable() {
            bool p2p_channel = true;
            uint64_t center_freq1 = 1000, center_freq2 = center_freq1 + 1;
            uint64_t bandwidth = 500;
            reservation_manager->addFrequencyChannel(p2p_channel, center_freq1, bandwidth);
            reservation_manager->addFrequencyChannel(p2p_channel, center_freq2, bandwidth);

            ReservationTable* table1 = reservation_manager->getReservationTable(0);
            ReservationTable* table2 = reservation_manager->getReservationTable(1);

            // Mark one slot as busy in table1.
            table1->mark(0, Reservation(MacId(0), Reservation::Action::BUSY));
            ReservationTable* least_utilized_table = reservation_manager->getLeastUtilizedP2PReservationTable();
            CPPUNIT_ASSERT_EQUAL(table2, least_utilized_table); // table2 contains more idle slots now.

            // Now mark *two* slots busy in table2.
            table2->mark(0, Reservation(MacId(0), Reservation::Action::BUSY));
            table2->mark(1, Reservation(MacId(0), Reservation::Action::BUSY));
            least_utilized_table = reservation_manager->getLeastUtilizedP2PReservationTable();
            CPPUNIT_ASSERT_EQUAL(table1, least_utilized_table); // table1 contains more idle slots now.
		}
		
		void testGetSortedReservationTables() {
			// Should throw error if no reservation tables are present.
			bool exception_occurred = false;
			try {
				reservation_manager->getSortedP2PReservationTables();
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
			// Add reservation tables.
			bool p2p_channel = true;
			uint64_t center_freq1 = 1000, center_freq2 = center_freq1 + 1, center_freq3 = center_freq2 + 1;
			uint64_t bandwidth = 500;
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq1, bandwidth);
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq2, bandwidth);
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq3, bandwidth);
			
			ReservationTable* table1 = reservation_manager->getReservationTable(0); // No busy slots.
			ReservationTable* table2 = reservation_manager->getReservationTable(1); // Three busy slots.
			table2->mark(0, Reservation(MacId(0), Reservation::Action::BUSY));
			table2->mark(1, Reservation(MacId(0), Reservation::Action::BUSY));
			table2->mark(2, Reservation(MacId(0), Reservation::Action::BUSY));
			ReservationTable* table3 = reservation_manager->getReservationTable(2); // Two busy slots.
			table3->mark(0, Reservation(MacId(0), Reservation::Action::BUSY));
			table3->mark(1, Reservation(MacId(0), Reservation::Action::BUSY));
			
			auto queue = reservation_manager->getSortedP2PReservationTables();
			ReservationTable* table = queue.top();
			CPPUNIT_ASSERT(table == table1);
			queue.pop();
			table = queue.top();
			CPPUNIT_ASSERT(table == table3);
			queue.pop();
			table = queue.top();
			CPPUNIT_ASSERT(table == table2);
			queue.pop();
			CPPUNIT_ASSERT_EQUAL(true, queue.empty());
		}
	
	CPPUNIT_TEST_SUITE(ReservationManagerTests);
		CPPUNIT_TEST(testAddFreqChannel);
		CPPUNIT_TEST(testUpdate);
        CPPUNIT_TEST(testGetLeastUtilizedReservationTable);
		CPPUNIT_TEST(testGetSortedReservationTables);
	CPPUNIT_TEST_SUITE_END();
};