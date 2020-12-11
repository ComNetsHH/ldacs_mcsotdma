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
				for (size_t i = 0; i < 2*horizon; i++) {
					if (i < horizon)
						estimator->reportBroadcast(id);
					estimator->update();
					if (i < horizon) // Broadcasts reported every slot.
						CPPUNIT_ASSERT_EQUAL(1.0, estimator->getContentionEstimate(id));
					else {
						double j = (double) i,
					           l = (double) horizon;
						double decrease = (j - (l-1))*(1.0/l); // decrease by 1/l for every i exceeding horizon
						CPPUNIT_ASSERT_EQUAL(1.0 - decrease, estimator->getContentionEstimate(id));
					}
				}
			}
		
		CPPUNIT_TEST_SUITE(ContentionEstimatorTests);
			CPPUNIT_TEST(testEstimator);
		CPPUNIT_TEST_SUITE_END();
	};
	
}