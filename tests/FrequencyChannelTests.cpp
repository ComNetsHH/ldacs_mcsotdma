//
// Created by Sebastian Lindner on 14.10.20.
//

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
			CPPUNIT_ASSERT_EQUAL(true, channel->isPointToPointChannel());
			CPPUNIT_ASSERT_EQUAL(false, channel->isBroadcastChannel());
		}

		void testEquality() {
			FrequencyChannel* other = new FrequencyChannel(channel->isPointToPointChannel(),
			                                               channel->getCenterFrequency(), channel->getBandwidth());
			CPPUNIT_ASSERT_EQUAL(true, *channel == *other);
			delete other;
			other = new FrequencyChannel(!channel->isPointToPointChannel(), channel->getCenterFrequency(),
			                             channel->getBandwidth());
			CPPUNIT_ASSERT_EQUAL(false, *channel == *other);
			delete other;
			other = new FrequencyChannel(channel->isPointToPointChannel(), channel->getCenterFrequency() + 1,
			                             channel->getBandwidth());
			CPPUNIT_ASSERT_EQUAL(false, *channel == *other);
			delete other;
			other = new FrequencyChannel(channel->isPointToPointChannel(), channel->getCenterFrequency(),
			                             channel->getBandwidth() - 1);
			CPPUNIT_ASSERT_EQUAL(false, *channel == *other);
			delete other;
		}

		void testBlacklisting() {
			CPPUNIT_ASSERT_EQUAL(false, channel->isBlacklisted());
			channel->setBlacklisted(true);
			CPPUNIT_ASSERT_EQUAL(true, channel->isBlacklisted());
		}

	CPPUNIT_TEST_SUITE(FrequencyChannelTests);
			CPPUNIT_TEST(testGetCenterFreq);
			CPPUNIT_TEST(testCheckP2P);
			CPPUNIT_TEST(testEquality);
			CPPUNIT_TEST(testBlacklisting);
		CPPUNIT_TEST_SUITE_END();
	};

}