//
// Created by Sebastian Lindner on 11.12.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ContentionEstimator.hpp"
#include <MacId.hpp>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class ContentionEstimatorTests : public CppUnit::TestFixture {
	private:
		unsigned int horizon = 8;
		ContentionEstimator* estimator;
		MacId id = MacId(42);

	public:
		void setUp() override {
			estimator = new ContentionEstimator(horizon);
		}

		void tearDown() override {
			delete estimator;
		}

		void testEstimator() {
			for (size_t i = 0; i < 2 * horizon; i++) {
				if (i < horizon)
					estimator->reportNonBeaconBroadcast(id, 0);
				estimator->onSlotEnd(0);
				if (i < horizon) // Broadcasts reported every slot.
					CPPUNIT_ASSERT_EQUAL(1.0, estimator->getContentionEstimate(id));
				else {
					double j = (double) i,
							l = (double) horizon;
					double decrease = (j - (l - 1)) * (1.0 / l); // decrease by 1/l for every i exceeding horizon
					CPPUNIT_ASSERT_EQUAL(1.0 - decrease, estimator->getContentionEstimate(id));
				}
			}
		}

		void testGetNumActiveNeighbors() {
			MacId other_id = MacId(id.getId() + 1);
			CPPUNIT_ASSERT_EQUAL(uint(0), estimator->getNumActiveNeighbors());
			for (size_t i = 0; i < horizon / 2; i++) {
				estimator->reportNonBeaconBroadcast(id, 0);
				estimator->reportNonBeaconBroadcast(other_id, 0);
				estimator->onSlotEnd(0);
			}
			CPPUNIT_ASSERT_EQUAL(uint(2), estimator->getNumActiveNeighbors());
			for (size_t i = 0; i < horizon; i++) {
				estimator->reportNonBeaconBroadcast(id, 0);
				estimator->onSlotEnd(0);
			}
			CPPUNIT_ASSERT_EQUAL(uint(1), estimator->getNumActiveNeighbors());
			for (size_t i = 0; i < horizon; i++)
				estimator->onSlotEnd(0);
			CPPUNIT_ASSERT_EQUAL(uint(0), estimator->getNumActiveNeighbors());
		}

		void getGetAverageBroadcastRate() {
			MacId other_id = MacId(id.getId() + 1);
			CPPUNIT_ASSERT_EQUAL(0.0, estimator->getAverageNonBeaconBroadcastRate());
			for (size_t i = 0; i < horizon; i++) {
				if (i % 2 == 0)
					estimator->reportNonBeaconBroadcast(id, 0);
				estimator->onSlotEnd(0);
			}
			CPPUNIT_ASSERT_EQUAL(.5, estimator->getAverageNonBeaconBroadcastRate());
		}

		void testBroadcastInterval() {
			unsigned int beacon_interval = 3;
			MacId other_id = MacId(43);
			for (unsigned int t = 0; t < 3*beacon_interval; t++) {
				if (t % beacon_interval == 0)
					estimator->reportNonBeaconBroadcast(other_id, t);
				estimator->onSlotEnd(t);
			}
			CPPUNIT_ASSERT_EQUAL(beacon_interval, estimator->broadcast_interval_per_id.find(other_id)->second);
		}

		void testChannelAccessProb() {
			// Observe an active neighbor.
			unsigned int beacon_interval = 3;
			MacId other_id = MacId(43);
			estimator->reportNonBeaconBroadcast(other_id, 0);
			for (unsigned int t = 0; t < beacon_interval - 1; t++)
				estimator->onSlotEnd(t);
			estimator->reportNonBeaconBroadcast(other_id, beacon_interval);
			CPPUNIT_ASSERT_EQUAL(beacon_interval, estimator->broadcast_interval_per_id.find(other_id)->second);
			// Now progress in time and check the channel access probabilities.
			for (unsigned int t = 0; t <= beacon_interval; t++) {
				unsigned int current_slot = beacon_interval + t;
				double expected_prob;
				if (t == 0)
					expected_prob = 0.0;
				else if (t == 1)
					expected_prob = 1.0/3.0;
				else if (t == 2)
					expected_prob = 2.0/3.0;
				else if (t == 3)
					expected_prob = 1.0;
				else
					throw std::runtime_error("handcrafted test doesn't work for this many time slots!");
				double observed_prob = estimator->getChannelAccessProbability(other_id, current_slot);
				CPPUNIT_ASSERT_EQUAL(expected_prob, observed_prob);
				estimator->onSlotEnd(current_slot);
			}
			// Cap out at 100%.
			for (unsigned int current_slot = 2*beacon_interval + 1; current_slot < 3*beacon_interval; current_slot++) {
				estimator->onSlotEnd(current_slot);
				CPPUNIT_ASSERT_EQUAL(1.0, estimator->getChannelAccessProbability(other_id, current_slot));
			}
		}

		void testEraseInactiveNeighbors() {
			// Observe an active neighbor.
			unsigned int beacon_interval = 3;
			MacId other_id = MacId(43);
			unsigned int current_slot = 0;
			estimator->reportNonBeaconBroadcast(other_id, current_slot);
			estimator->onSlotEnd(current_slot);
			for (unsigned int t = 1; t <= beacon_interval; t++) {
				current_slot++;
				if (t == beacon_interval)
					estimator->reportNonBeaconBroadcast(other_id, current_slot);
				estimator->onSlotEnd(current_slot);
			}
			CPPUNIT_ASSERT_EQUAL(beacon_interval, estimator->broadcast_interval_per_id.find(other_id)->second);

			// Now progress past the contention window.
			for (unsigned int t = 0; current_slot < horizon + beacon_interval; t++) {
				current_slot++;
				estimator->onSlotEnd(current_slot);
				CPPUNIT_ASSERT(estimator->broadcast_interval_per_id.find(other_id) != estimator->broadcast_interval_per_id.end());
				CPPUNIT_ASSERT(estimator->last_broadcast_per_id.find(other_id) != estimator->last_broadcast_per_id.end());
			}
			current_slot++;
			estimator->onSlotEnd(current_slot);
			CPPUNIT_ASSERT(estimator->broadcast_interval_per_id.find(other_id) == estimator->broadcast_interval_per_id.end());
			CPPUNIT_ASSERT(estimator->last_broadcast_per_id.find(other_id) == estimator->last_broadcast_per_id.end());
		}

	CPPUNIT_TEST_SUITE(ContentionEstimatorTests);
		CPPUNIT_TEST(testEstimator);
		CPPUNIT_TEST(testGetNumActiveNeighbors);
		CPPUNIT_TEST(getGetAverageBroadcastRate);
		CPPUNIT_TEST(testBroadcastInterval);
		CPPUNIT_TEST(testChannelAccessProb);
		CPPUNIT_TEST(testEraseInactiveNeighbors);
	CPPUNIT_TEST_SUITE_END();
	};

}