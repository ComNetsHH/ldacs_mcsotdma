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
					estimator->reportNonBeaconBroadcast(id);
				estimator->update(1);
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
				estimator->reportNonBeaconBroadcast(id);
				estimator->reportNonBeaconBroadcast(other_id);
				estimator->update(1);
			}
			CPPUNIT_ASSERT_EQUAL(uint(2), estimator->getNumActiveNeighbors());
			for (size_t i = 0; i < horizon; i++) {
				estimator->reportNonBeaconBroadcast(id);
				estimator->update(1);
			}
			CPPUNIT_ASSERT_EQUAL(uint(1), estimator->getNumActiveNeighbors());
			for (size_t i = 0; i < horizon; i++)
				estimator->update(1);
			CPPUNIT_ASSERT_EQUAL(uint(0), estimator->getNumActiveNeighbors());
		}

		void getGetAverageBroadcastRate() {
			MacId other_id = MacId(id.getId() + 1);
			CPPUNIT_ASSERT_EQUAL(0.0, estimator->getAverageBroadcastRate());
			for (size_t i = 0; i < horizon; i++) {
				if (i % 2 == 0)
					estimator->reportNonBeaconBroadcast(id);
				estimator->update(1);
			}
			CPPUNIT_ASSERT_EQUAL(.5, estimator->getAverageBroadcastRate());
		}

	CPPUNIT_TEST_SUITE(ContentionEstimatorTests);
			CPPUNIT_TEST(testEstimator);
			CPPUNIT_TEST(testGetNumActiveNeighbors);
			CPPUNIT_TEST(getGetAverageBroadcastRate);
		CPPUNIT_TEST_SUITE_END();
	};

}