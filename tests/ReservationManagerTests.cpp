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
				const FrequencyChannel* channel = reservation_manager->getFreqChannelByIndex(0);
			} catch(const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			
			// Add it.
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq, bandwidth);
			
			// Now we should be able to get both reservation table and frequency channel.
			exception_thrown = false;
			try {
				const FrequencyChannel* channel = reservation_manager->getFreqChannelByIndex(0);
				const ReservationTable* table = reservation_manager->getReservationTableByIndex(0);
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
			CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTableByIndex(0)->getCurrentSlot() == now);
			CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTableByIndex(1)->getCurrentSlot() == now);
			
			uint64_t num_slots = 5;
			reservation_manager->update(num_slots);
			CPPUNIT_ASSERT_EQUAL(false, reservation_manager->getReservationTableByIndex(0)->getCurrentSlot() == now);
			CPPUNIT_ASSERT_EQUAL(false, reservation_manager->getReservationTableByIndex(1)->getCurrentSlot() == now);
			
			now += num_slots;
			
			CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTableByIndex(0)->getCurrentSlot() == now);
			CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTableByIndex(1)->getCurrentSlot() == now);
		}

		void testGetLeastUtilizedReservationTable() {
            bool p2p_channel = true;
            uint64_t center_freq1 = 1000, center_freq2 = center_freq1 + 1;
            uint64_t bandwidth = 500;
            reservation_manager->addFrequencyChannel(p2p_channel, center_freq1, bandwidth);
            reservation_manager->addFrequencyChannel(p2p_channel, center_freq2, bandwidth);

            ReservationTable* table1 = reservation_manager->getReservationTableByIndex(0);
            ReservationTable* table2 = reservation_manager->getReservationTableByIndex(1);

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
			// Should get an empty queue if no tables are present.
			CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getSortedP2PReservationTables().empty());
			// Add reservation tables.
			bool p2p_channel = true;
			uint64_t center_freq1 = 1000, center_freq2 = center_freq1 + 1, center_freq3 = center_freq2 + 1;
			uint64_t bandwidth = 500;
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq1, bandwidth);
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq2, bandwidth);
			reservation_manager->addFrequencyChannel(p2p_channel, center_freq3, bandwidth);
			
			ReservationTable* table1 = reservation_manager->getReservationTableByIndex(0); // No busy slots.
			ReservationTable* table2 = reservation_manager->getReservationTableByIndex(1); // Three busy slots.
			table2->mark(0, Reservation(MacId(0), Reservation::Action::BUSY));
			table2->mark(1, Reservation(MacId(0), Reservation::Action::BUSY));
			table2->mark(2, Reservation(MacId(0), Reservation::Action::BUSY));
			ReservationTable* table3 = reservation_manager->getReservationTableByIndex(2); // Two busy slots.
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
		
		void testGetByPointer() {
			uint64_t freq1 = 1000, freq2 = 2000, bc_freq = 3000, bandwidth = 500;
			reservation_manager->addFrequencyChannel(true, freq1, bandwidth);
			reservation_manager->addFrequencyChannel(true, freq2, bandwidth);
			reservation_manager->addFrequencyChannel(false, bc_freq, bandwidth);
			FrequencyChannel *channel1 = reservation_manager->getFreqChannelByIndex(0),
								*channel2 = reservation_manager->getFreqChannelByIndex(1),
								*bc_channel = reservation_manager->getBroadcastFreqChannel();
			ReservationTable *table1 = reservation_manager->getReservationTableByIndex(0),
								*table2 = reservation_manager->getReservationTableByIndex(1),
								*bc_table = reservation_manager->getBroadcastReservationTable();
			CPPUNIT_ASSERT(channel1 == reservation_manager->getFreqChannel(table1));
			CPPUNIT_ASSERT(channel2 == reservation_manager->getFreqChannel(table2));
			CPPUNIT_ASSERT(bc_channel == reservation_manager->getFreqChannel(bc_table));
			
			CPPUNIT_ASSERT(table1 == reservation_manager->getReservationTable(channel1));
			CPPUNIT_ASSERT(table2 == reservation_manager->getReservationTable(channel2));
			CPPUNIT_ASSERT(bc_table == reservation_manager->getReservationTable(bc_channel));
		}
	
	CPPUNIT_TEST_SUITE(ReservationManagerTests);
		CPPUNIT_TEST(testAddFreqChannel);
		CPPUNIT_TEST(testUpdate);
        CPPUNIT_TEST(testGetLeastUtilizedReservationTable);
		CPPUNIT_TEST(testGetSortedReservationTables);
		CPPUNIT_TEST(testGetByPointer);
	CPPUNIT_TEST_SUITE_END();
};