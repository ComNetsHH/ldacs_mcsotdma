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
#include "../FrequencyChannel.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class FrequencyChannelTests : public CppUnit::TestFixture {
	private:
		FrequencyChannel* channel;

	public:
		void setUp() override {
			bool is_p2p = true;
			uint64_t center_frequency = uint64_t(1e9); // 1GHz.
			uint64_t bandwidth = uint64_t(20000); // 20kHz.
			channel = new FrequencyChannel(is_p2p, center_frequency, bandwidth);
		}

		void tearDown() override {
			delete channel;
		}

		void testGetCenterFreq() {
			CPPUNIT_ASSERT_EQUAL(uint64_t(1e9), channel->getCenterFrequency());
			CPPUNIT_ASSERT_EQUAL(uint64_t(20000), channel->getBandwidth());
		}

		void testCheckP2P() {
			CPPUNIT_ASSERT_EQUAL(true, channel->isPP());
			CPPUNIT_ASSERT_EQUAL(false, channel->isSH());
		}

		void testEquality() {
			FrequencyChannel* other = new FrequencyChannel(channel->isPP(),
			                                               channel->getCenterFrequency(), channel->getBandwidth());
			CPPUNIT_ASSERT_EQUAL(true, *channel == *other);
			delete other;
			other = new FrequencyChannel(!channel->isPP(), channel->getCenterFrequency(),
			                             channel->getBandwidth());
			CPPUNIT_ASSERT_EQUAL(false, *channel == *other);
			delete other;
			other = new FrequencyChannel(channel->isPP(), channel->getCenterFrequency() + 1,
			                             channel->getBandwidth());
			CPPUNIT_ASSERT_EQUAL(false, *channel == *other);
			delete other;
			other = new FrequencyChannel(channel->isPP(), channel->getCenterFrequency(),
			                             channel->getBandwidth() - 1);
			CPPUNIT_ASSERT_EQUAL(false, *channel == *other);
			delete other;
		}

		void testBlacklisting() {
			CPPUNIT_ASSERT_EQUAL(false, channel->isBlocked());
			channel->setBlacklisted(true);
			CPPUNIT_ASSERT_EQUAL(true, channel->isBlocked());
		}

	CPPUNIT_TEST_SUITE(FrequencyChannelTests);
			CPPUNIT_TEST(testGetCenterFreq);
			CPPUNIT_TEST(testCheckP2P);
			CPPUNIT_TEST(testEquality);
			CPPUNIT_TEST(testBlacklisting);
		CPPUNIT_TEST_SUITE_END();
	};

}