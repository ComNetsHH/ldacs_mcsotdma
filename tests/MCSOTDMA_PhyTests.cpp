//
// Created by seba on 1/28/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../MCSOTDMA_Phy.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class MCSOTDMA_PhyTests : public CppUnit::TestFixture {
	private:
		MacId own_id = MacId(42), communication_partner_id = MacId(43);
		uint64_t center_freq1;
		TestEnvironment* env;
		MCSOTDMA_Phy* phy;
		MCSOTDMA_Mac* mac;

	public:
		void setUp() override {
			env = new TestEnvironment(own_id, communication_partner_id);
			center_freq1 = env->center_frequency1;
			phy = env->phy_layer;
			mac = env->mac_layer;
		}

		void tearDown() override {
			delete env;
		}

		void testDiscardPacketWhenNoReceiverListens() {
//            coutd.setVerbose(true);
			auto *packet = new L2Packet();
			phy->onReception(packet, center_freq1);
			// Should've been discarded.
			CPPUNIT_ASSERT_EQUAL(size_t(0), phy->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(0), phy->statistic_num_missed_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(0), mac->statistic_num_packets_received);

			// Now destine it to us.
			auto* header = new L2HeaderBase(communication_partner_id, 0, 0, 0, 0);
			packet->addMessage(header, nullptr);
			auto* header2 = new L2HeaderUnicast(own_id, false, 0, 0, 0);
			packet->addMessage(header2, nullptr);
			phy->onReception(packet, center_freq1);
			// Should still be discarded.
			CPPUNIT_ASSERT_EQUAL(size_t(0), phy->statistic_num_received_packets);
			// But since we're the destination, it should count towards missed packets.
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy->statistic_num_missed_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(0), mac->statistic_num_packets_received);

			CPPUNIT_ASSERT_EQUAL(true, phy->rx_frequencies.empty());
			phy->tuneReceiver(center_freq1);
			CPPUNIT_ASSERT_EQUAL(false, phy->rx_frequencies.empty());

			phy->onReception(packet, center_freq1);
			// Should *not* have been discarded.
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac->statistic_num_packets_received);
			// And so the number of missed packets doesn't increase.
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy->statistic_num_missed_packets);

			phy->update(1);
			CPPUNIT_ASSERT_EQUAL(true, phy->rx_frequencies.empty());
			phy->onReception(packet, center_freq1);

			// Should again be discarded - no receiver is tuned *in this time slot*.
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(2), phy->statistic_num_missed_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac->statistic_num_packets_received);

//            coutd.setVerbose(false);
		}

	CPPUNIT_TEST_SUITE(MCSOTDMA_PhyTests);
			CPPUNIT_TEST(testDiscardPacketWhenNoReceiverListens);
		CPPUNIT_TEST_SUITE_END();
	};
}