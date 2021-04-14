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
			beacon_module = new BeaconModule(bc_table);
		}

		void tearDown() override {
			delete beacon_module;
			delete bc_table;
		}

		void testBeaconInterval() {
			double target_congestion = .45;
			double avg_broadcast_rate = 10.3;

			for (unsigned int min_beacon_gap = 0; min_beacon_gap < 10; min_beacon_gap++) {
				BeaconModule mod = BeaconModule(bc_table, min_beacon_gap);
				for (unsigned int num_active_neighbors = 1; num_active_neighbors < 1000; num_active_neighbors++) {
					unsigned int beacon_offset = mod.computeBeaconInterval(target_congestion, avg_broadcast_rate, num_active_neighbors);
					CPPUNIT_ASSERT(beacon_offset >= mod.min_beacon_offset);
					CPPUNIT_ASSERT(beacon_offset <= mod.max_beacon_offset);
				}
			}
		}

		CPPUNIT_TEST_SUITE(BeaconModuleTests);
			CPPUNIT_TEST(testBeaconInterval);
		CPPUNIT_TEST_SUITE_END();
	};

}