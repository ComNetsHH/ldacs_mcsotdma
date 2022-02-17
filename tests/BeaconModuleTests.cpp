//
// Created by seba on 4/14/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../BeaconModule.hpp"
#include "../BeaconPayload.hpp"
#include "../coutdebug.hpp"
#include <SimulatorPosition.hpp>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class BeaconModuleTests : public CppUnit::TestFixture {
	private:
		unsigned int horizon = 8;
		BeaconModule *beacon_module;
		ReservationTable *bc_table, *tx_table;
		uint32_t planning_horizon;

	public:
		void setUp() override {
			planning_horizon = 1024;
			bc_table = new ReservationTable(planning_horizon);
			tx_table = new ReservationTable(planning_horizon);
			beacon_module = new BeaconModule();
		}

		void tearDown() override {
			delete beacon_module;
			delete bc_table;
			delete tx_table;
		}

		void testBeaconInterval() {
			double target_congestion = .45;
			double avg_broadcast_rate = 10.3;

			for (unsigned int min_beacon_gap = 0; min_beacon_gap < 10; min_beacon_gap++) {
				BeaconModule mod = BeaconModule(min_beacon_gap, .45);
				unsigned int last_beacon_offset = mod.computeBeaconInterval(1); 
				for (unsigned int num_active_neighbors = 4; num_active_neighbors < 1000; num_active_neighbors++) {
					unsigned int beacon_offset = mod.computeBeaconInterval(num_active_neighbors);										
					CPPUNIT_ASSERT_GREATEREQUAL(mod.min_beacon_offset, beacon_offset);
					CPPUNIT_ASSERT_LESSEQUAL(mod.max_beacon_offset, beacon_offset);
					if (beacon_offset != mod.min_beacon_offset && beacon_offset != mod.max_beacon_offset)
						CPPUNIT_ASSERT(beacon_offset > last_beacon_offset);					
					last_beacon_offset = beacon_offset;
				}
			}
		}

		void testChooseNextBeaconSlot() {
			unsigned int beacon_offset = beacon_module->min_beacon_offset,
						 num_candidates = 1,
						 min_gap = beacon_module->min_beacon_gap;
			unsigned int next_slot = beacon_module->chooseNextBeaconSlot(beacon_offset, num_candidates, min_gap, bc_table, tx_table);
			CPPUNIT_ASSERT_EQUAL(beacon_offset, next_slot);

			num_candidates = 3;
			double next_slot_avg = 0;
			for (size_t i = 0; i < 100; i++)
				next_slot_avg += beacon_module->chooseNextBeaconSlot(beacon_offset, num_candidates, min_gap, bc_table, tx_table);
			next_slot_avg /= 100;
			CPPUNIT_ASSERT(next_slot_avg > beacon_offset && next_slot_avg < beacon_offset + num_candidates);
		}

		void testKeepGapPattern() {
			unsigned int beacon_offset = beacon_module->min_beacon_offset,
					num_candidates = 1,
					min_gap = 1;

			bc_table->mark(beacon_offset, Reservation(MacId(54), Reservation::RX_BEACON));
			unsigned int next_slot = beacon_module->chooseNextBeaconSlot(beacon_offset, num_candidates, min_gap, bc_table, tx_table);
			CPPUNIT_ASSERT_EQUAL(beacon_offset + min_gap + 1, next_slot);

			min_gap = 3;
			next_slot = beacon_module->chooseNextBeaconSlot(beacon_offset, num_candidates, min_gap, bc_table, tx_table);
			CPPUNIT_ASSERT_EQUAL(beacon_offset + min_gap + 1, next_slot);
		}

		void testBeaconMessage() {
			std::vector<unsigned int> marked_bc_slots = {2, 4, 13},
									  marked_p2p_slots = {12, 55, 65};
			ReservationTable p2p_table = ReservationTable(bc_table->getPlanningHorizon());
			for (auto t : marked_bc_slots)
				bc_table->mark(t, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX));
			for (auto t : marked_p2p_slots)
				p2p_table.mark(t, Reservation(MacId(12), Reservation::TX));

			BeaconPayload payload = BeaconPayload();
			uint64_t bc_freq = 1000, p2p_freq = 2000;
			payload.encode(1000, bc_table);
			payload.encode(2000, &p2p_table);

			const auto &bc_vec = payload.local_reservations[bc_freq];
			CPPUNIT_ASSERT_EQUAL(bc_table->countReservedTxSlots(SYMBOLIC_LINK_ID_BROADCAST), bc_vec.size());
			for (size_t i = 0; i < marked_bc_slots.size(); i++)
				CPPUNIT_ASSERT_EQUAL(marked_bc_slots.at(i), bc_vec.at(i).first);

			const auto &p2p_vec = payload.local_reservations[p2p_freq];
			CPPUNIT_ASSERT_EQUAL(p2p_table.countReservedTxSlots(MacId(12)), p2p_vec.size());
			for (size_t i = 0; i < marked_p2p_slots.size(); i++)
				CPPUNIT_ASSERT_EQUAL(marked_p2p_slots.at(i), p2p_vec.at(i).first);
		}

		void testMinBeaconOffset() {
			double target_congestion = .45;
			double avg_broadcast_rate = 0.0;
			unsigned int num_active_neighbors = 0;
			unsigned int beacon_interval = beacon_module->computeBeaconInterval(num_active_neighbors);
			CPPUNIT_ASSERT_EQUAL(beacon_module->min_beacon_offset, beacon_interval);
			// change min beacon offset
			beacon_module->setMinBeaconInterval(beacon_module->min_beacon_offset * 2);			
			beacon_interval = beacon_module->computeBeaconInterval(num_active_neighbors);
			CPPUNIT_ASSERT_EQUAL(beacon_module->min_beacon_offset, beacon_interval);			
		}

		void testMaxBeaconOffset() {
			double target_congestion = .45;
			double avg_broadcast_rate = .99;
			unsigned int num_active_neighbors = 1000;		
			unsigned int beacon_interval = beacon_module->computeBeaconInterval(num_active_neighbors);
			CPPUNIT_ASSERT_GREATER(beacon_module->min_beacon_offset, beacon_interval);
			// change max beacon offset
			beacon_module->setMaxBeaconInterval(beacon_module->getMinBeaconInterval());
			beacon_interval = beacon_module->computeBeaconInterval(num_active_neighbors);
			CPPUNIT_ASSERT_EQUAL(beacon_module->min_beacon_offset, beacon_interval);			
			CPPUNIT_ASSERT_EQUAL(beacon_module->max_beacon_offset, beacon_interval);	
		}

		void testCongestionLevel() {
			auto tmp_res_table = ReservationTable(128);
			FrequencyChannel channel = FrequencyChannel(false, 100, 500);
			tmp_res_table.linkFrequencyChannel(&channel);
			// no active links -> no congestion
			auto msg = beacon_module->generateBeacon({}, &tmp_res_table, SimulatorPosition(), 0, 20);			
			CPPUNIT_ASSERT_EQUAL(L2HeaderBeacon::CongestionLevel::uncongested, msg.first->congestion_level);						
			delete msg.first;
			delete msg.second;
			// <25% active links -> no congestion
			msg = beacon_module->generateBeacon({}, &tmp_res_table, SimulatorPosition(), 4, 20);
			CPPUNIT_ASSERT_EQUAL(L2HeaderBeacon::CongestionLevel::uncongested, msg.first->congestion_level);
			delete msg.first;
			delete msg.second;
			// 25%-49% active links -> slight congestion
			msg = beacon_module->generateBeacon({}, &tmp_res_table, SimulatorPosition(), 5, 20);
			CPPUNIT_ASSERT_EQUAL(L2HeaderBeacon::CongestionLevel::slightly_congested, msg.first->congestion_level);
			delete msg.first;
			delete msg.second;
			msg = beacon_module->generateBeacon({}, &tmp_res_table, SimulatorPosition(), 9, 20);
			CPPUNIT_ASSERT_EQUAL(L2HeaderBeacon::CongestionLevel::slightly_congested, msg.first->congestion_level);
			delete msg.first;
			delete msg.second;
			// 50%-74% active links -> moderate congestion
			msg = beacon_module->generateBeacon({}, &tmp_res_table, SimulatorPosition(), 10, 20);
			CPPUNIT_ASSERT_EQUAL(L2HeaderBeacon::CongestionLevel::moderately_congested, msg.first->congestion_level);
			delete msg.first;
			delete msg.second;
			msg = beacon_module->generateBeacon({}, &tmp_res_table, SimulatorPosition(), 14, 20);
			CPPUNIT_ASSERT_EQUAL(L2HeaderBeacon::CongestionLevel::moderately_congested, msg.first->congestion_level);
			delete msg.first;
			delete msg.second;
			// >75% active links -> congestion
			msg = beacon_module->generateBeacon({}, &tmp_res_table, SimulatorPosition(), 15, 20);
			CPPUNIT_ASSERT_EQUAL(L2HeaderBeacon::CongestionLevel::congested, msg.first->congestion_level);
			delete msg.first;
			delete msg.second;
			msg = beacon_module->generateBeacon({}, &tmp_res_table, SimulatorPosition(), 20, 20);
			CPPUNIT_ASSERT_EQUAL(L2HeaderBeacon::CongestionLevel::congested, msg.first->congestion_level);
			delete msg.first;
			delete msg.second;
			CPPUNIT_ASSERT_THROW(beacon_module->generateBeacon({}, &tmp_res_table, SimulatorPosition(), 21, 20), std::invalid_argument);			
		}

		CPPUNIT_TEST_SUITE(BeaconModuleTests);
			CPPUNIT_TEST(testBeaconInterval);
			CPPUNIT_TEST(testChooseNextBeaconSlot);
			CPPUNIT_TEST(testKeepGapPattern);
			CPPUNIT_TEST(testBeaconMessage);
			CPPUNIT_TEST(testMinBeaconOffset);
			CPPUNIT_TEST(testMaxBeaconOffset);
			CPPUNIT_TEST(testCongestionLevel);			
		CPPUNIT_TEST_SUITE_END();
	};

}