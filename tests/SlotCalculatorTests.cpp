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
#include "../SlotCalculator.hpp"
#include <cmath>

namespace TUHH_INTAIRNET_MCSOTDMA {

	class SlotCalculatorTests : public CppUnit::TestFixture {
	private:		

	public:
		void setUp() override {			
		}

		void tearDown() override {			
		}

		void testAlternatingBursts() {
			int start_slot_offset = 5;
			int num_forward_bursts = 1;
			int num_reverse_bursts = 1;
			int period = 2;
			int timeout = 7;
			auto tx_rx_slots = SlotCalculator::calculateAlternatingBursts(start_slot_offset, num_forward_bursts, num_reverse_bursts, period, timeout);
			const auto &tx_slots = tx_rx_slots.first;
			const auto &rx_slots = tx_rx_slots.second;
			CPPUNIT_ASSERT_EQUAL(size_t(timeout), tx_slots.size());			
			CPPUNIT_ASSERT_EQUAL(tx_slots.size(), rx_slots.size());
			for (size_t i = 0; i < tx_slots.size(); i++) {
				// every rx slot is exactly 5*2^n slots later than its corresponding tx slot
				CPPUNIT_ASSERT_EQUAL((int) (tx_slots.at(i) + 5*std::pow(2,period)), rx_slots.at(i));
			}
		}		

	CPPUNIT_TEST_SUITE(SlotCalculatorTests);
		CPPUNIT_TEST(testAlternatingBursts);			
	CPPUNIT_TEST_SUITE_END();
	};

}