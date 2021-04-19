//
// Created by seba on 4/14/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../BeaconModule.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class BeaconModuleTests : public CppUnit::TestFixture {
	private:
		unsigned int horizon = 8;
		BeaconModule *beacon_module;
		ReservationTable *bc_table;
		uint32_t planning_horizon;

	public:
		void setUp() override {
			planning_horizon = 1024;
			bc_table = new ReservationTable(planning_horizon);
			beacon_module = new BeaconModule();
			beacon_module->setBcReservationTable(bc_table);
		}

		void tearDown() override {
			delete beacon_module;
			delete bc_table;
		}

		void testBeaconInterval() {
			double target_congestion = .45;
			double avg_broadcast_rate = 10.3;

			for (unsigned int min_beacon_gap = 0; min_beacon_gap < 10; min_beacon_gap++) {
				BeaconModule mod = BeaconModule(bc_table, min_beacon_gap, .45);
				for (unsigned int num_active_neighbors = 1; num_active_neighbors < 1000; num_active_neighbors++) {
					unsigned int beacon_offset = mod.computeBeaconInterval(target_congestion, avg_broadcast_rate, num_active_neighbors);
					CPPUNIT_ASSERT(beacon_offset >= mod.MIN_BEACON_OFFSET);
					CPPUNIT_ASSERT(beacon_offset <= mod.MAX_BEACON_OFFSET);
				}
			}
		}

		void testChooseNextBeaconSlot() {
			unsigned int beacon_offset = beacon_module->MIN_BEACON_OFFSET,
						 num_candidates = 1,
						 min_gap = beacon_module->min_beacon_gap;
			unsigned int next_slot = beacon_module->chooseNextBeaconSlot(beacon_offset, num_candidates, min_gap);
			CPPUNIT_ASSERT_EQUAL(beacon_offset, next_slot);

			num_candidates = 3;
			double next_slot_avg = 0;
			for (size_t i = 0; i < 100; i++)
				next_slot_avg += beacon_module->chooseNextBeaconSlot(beacon_offset, num_candidates, min_gap);
			next_slot_avg /= 100;
			CPPUNIT_ASSERT(next_slot_avg > beacon_offset && next_slot_avg < beacon_offset + num_candidates);
		}

		void testKeepGapPattern() {
			unsigned int beacon_offset = beacon_module->MIN_BEACON_OFFSET,
					num_candidates = 1,
					min_gap = 1;

			bc_table->mark(beacon_offset, Reservation(MacId(54), Reservation::RX_BEACON));
			unsigned int next_slot = beacon_module->chooseNextBeaconSlot(beacon_offset, num_candidates, min_gap);
			CPPUNIT_ASSERT_EQUAL(beacon_offset + min_gap + 1, next_slot);

			min_gap = 3;
			next_slot = beacon_module->chooseNextBeaconSlot(beacon_offset, num_candidates, min_gap);
			CPPUNIT_ASSERT_EQUAL(beacon_offset + min_gap + 1, next_slot);
		}

		CPPUNIT_TEST_SUITE(BeaconModuleTests);
			CPPUNIT_TEST(testBeaconInterval);
			CPPUNIT_TEST(testChooseNextBeaconSlot);
			CPPUNIT_TEST(testKeepGapPattern);
		CPPUNIT_TEST_SUITE_END();
	};

}