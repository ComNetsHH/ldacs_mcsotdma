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
			bool p2p_channel = false;
			uint64_t center_freq = 1000;
			uint64_t bandwidth = 500;
			
			// Fetching it now should throw an exception.
			bool exception_thrown = false;
			try {
				const FrequencyChannel& channel = manager->getFreqChannel(center_freq);
			} catch(const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			
			// Add it.
			manager->addFrequencyChannel(p2p_channel, center_freq, bandwidth);
			
			// Now we should be able to get both reservation table and frequency channel.
			exception_thrown = false;
			try {
				const FrequencyChannel& channel = manager->getFreqChannel(center_freq);
				const ReservationTable& table = manager->getReservationTable(center_freq);
			} catch(const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
		}
		
		void testRemoveFreqChannel() {
			bool p2p_channel = false;
			uint64_t center_freq = 1000;
			uint64_t bandwidth = 500;
			manager->addFrequencyChannel(p2p_channel, center_freq, bandwidth);
			
			manager->removeFrequencyChannel(center_freq);
			bool exception_thrown = false;
			try {
				const FrequencyChannel& channel = manager->getFreqChannel(center_freq);
			} catch(const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
		}
		
		void testUpdate() {
			bool p2p_channel = false;
			uint64_t center_freq1 = 1000, center_freq2 = center_freq1 + 1;
			uint64_t bandwidth = 500;
			manager->addFrequencyChannel(p2p_channel, center_freq1, bandwidth);
			manager->addFrequencyChannel(p2p_channel, center_freq2, bandwidth);
			Timestamp now = Timestamp();
			CPPUNIT_ASSERT_EQUAL(true, manager->getReservationTable(center_freq1).getCurrentSlot() == now);
			CPPUNIT_ASSERT_EQUAL(true, manager->getReservationTable(center_freq2).getCurrentSlot() == now);
			
			uint64_t num_slots = 5;
			manager->update(num_slots);
			CPPUNIT_ASSERT_EQUAL(false, manager->getReservationTable(center_freq1).getCurrentSlot() == now);
			CPPUNIT_ASSERT_EQUAL(false, manager->getReservationTable(center_freq2).getCurrentSlot() == now);
			
			now += num_slots;
			
			CPPUNIT_ASSERT_EQUAL(true, manager->getReservationTable(center_freq1).getCurrentSlot() == now);
			CPPUNIT_ASSERT_EQUAL(true, manager->getReservationTable(center_freq2).getCurrentSlot() == now);
		}
	
	CPPUNIT_TEST_SUITE(ReservationManagerTests);
		CPPUNIT_TEST(testAddFreqChannel);
		CPPUNIT_TEST(testRemoveFreqChannel);
		CPPUNIT_TEST(testUpdate);
	CPPUNIT_TEST_SUITE_END();
};