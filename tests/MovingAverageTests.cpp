//
// Created by Sebastian Lindner on 10.12.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class MovingAverageTests : public CppUnit::TestFixture {
	private:
		MovingAverage* avg;
		unsigned int size = 20;

	public:
		void setUp() override {
			avg = new MovingAverage(size);
		}

		void tearDown() override {
			delete avg;
		}

		void testAvg() {
			CPPUNIT_ASSERT_EQUAL(0.0, avg->get());
			unsigned int initial_bits = 10;
			unsigned int num_bits = initial_bits;
			double sum = 0;
			// Fill up the window.
			for (size_t i = 0; i < size; i++) {
				avg->put(num_bits);
				sum += num_bits;
				num_bits += initial_bits;
				CPPUNIT_ASSERT_EQUAL(sum / (i + 1), avg->get());
			}
			// Now it's full, so the next input will kick out the first value.
			avg->put(num_bits);
			sum -= initial_bits;
			sum += num_bits;
			CPPUNIT_ASSERT_EQUAL(sum / (size), avg->get());
		}

	CPPUNIT_TEST_SUITE(MovingAverageTests);
			CPPUNIT_TEST(testAvg);
		CPPUNIT_TEST_SUITE_END();
	};
}