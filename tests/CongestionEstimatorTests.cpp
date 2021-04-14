//
// Created by Sebastian Lindner on 4/14/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../CongestionEstimator.hpp"
#include <MacId.hpp>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class CongestionEstimatorTests : public CppUnit::TestFixture {
	private:
		unsigned int horizon ;
		CongestionEstimator *estimator;

	public:
		void setUp() override {
			horizon = 8;
			estimator = new CongestionEstimator(horizon);
		}

		void tearDown() override {
			delete estimator;
		}

		void testEstimator() {
			CPPUNIT_ASSERT_EQUAL(0.0, estimator->getCongestion());
			for (unsigned int t = 0; t < horizon; t++) {
				estimator->reportBroadcast(MacId(t));
				estimator->update(1);
			}
			CPPUNIT_ASSERT_EQUAL(1.0, estimator->getCongestion());
			CPPUNIT_ASSERT_EQUAL(horizon, estimator->getNumActiveNeighbors());
			for (unsigned int t = 0; t < horizon; t++)
				CPPUNIT_ASSERT_EQUAL(true, estimator->isActive(MacId(t)));
			CPPUNIT_ASSERT_EQUAL(false, estimator->isActive(MacId(horizon)));
			CPPUNIT_ASSERT_THROW(estimator->update(1), std::runtime_error);
			estimator->reset(horizon);
			CPPUNIT_ASSERT_EQUAL(0.0, estimator->getCongestion());

			// Now there'll be broadcasts only half the time.
			CPPUNIT_ASSERT_EQUAL(true, horizon % 2 == 0);
			for (unsigned int t = 0; t < horizon / 2; t++) {
				estimator->reportBroadcast(MacId(t));
				estimator->update(1);
			}
			// Getting the congestion returns the congestion *so far*.
			CPPUNIT_ASSERT_EQUAL(1.0, estimator->getCongestion());
			CPPUNIT_ASSERT_EQUAL(horizon, estimator->getNumActiveNeighbors());
			// No more broadcasts.
			for (unsigned int t = 0; t < horizon / 2; t++)
				estimator->update(1);
			CPPUNIT_ASSERT_EQUAL(.5, estimator->getCongestion());
			CPPUNIT_ASSERT_EQUAL(horizon, estimator->getNumActiveNeighbors());

			// Now one full horizon of zero broadcasts.
			estimator->reset(horizon);
			for (unsigned int t = 0; t < horizon; t++)
				estimator->update(1);
			CPPUNIT_ASSERT_EQUAL(.0, estimator->getCongestion());
			// Should still report the 50% of users from last round.
			CPPUNIT_ASSERT_EQUAL(horizon / 2, estimator->getNumActiveNeighbors());

			// Another empty round.
			estimator->reset(horizon);
			for (unsigned int t = 0; t < horizon; t++)
				estimator->update(1);
			CPPUNIT_ASSERT_EQUAL(.0, estimator->getCongestion());
			// Now zero users should be reported as active.
			CPPUNIT_ASSERT_EQUAL(uint32_t(0), estimator->getNumActiveNeighbors());
		}

		CPPUNIT_TEST_SUITE(CongestionEstimatorTests);
			CPPUNIT_TEST(testEstimator);
		CPPUNIT_TEST_SUITE_END();
	};

}