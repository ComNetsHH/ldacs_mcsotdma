// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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