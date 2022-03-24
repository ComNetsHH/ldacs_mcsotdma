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
			center_freq1 = env->p2p_freq_1;
			phy = env->phy_layer;
			mac = env->mac_layer;
		}

		void tearDown() override {
			delete env;
		}

		void testDiscardPacketWhenNoReceiverListens() {
//            coutd.setVerbose(true);
			auto *packet_empty = new L2Packet();
			phy->onReception(packet_empty, center_freq1);
			// Should've been discarded.
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) phy->stat_num_packets_rcvd.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) phy->stat_num_packets_missed.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_packets_rcvd.get());
			delete packet_empty;

			// Now destine it to us.
			auto *packet_destined_to_us = new L2Packet();
			packet_destined_to_us->addMessage(new L2HeaderBase(communication_partner_id, 0, 0, 0, 0), nullptr);
			packet_destined_to_us->addMessage(new L2HeaderUnicast(own_id, false, 0, 0), nullptr);
			phy->onReception(packet_destined_to_us, center_freq1);
			// Should still be discarded.
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) phy->stat_num_packets_rcvd.get());
			// But since we're the destination, it should count towards missed packets.
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) phy->stat_num_packets_missed.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_packets_rcvd.get());

			// Now tune the receiver.
			CPPUNIT_ASSERT_EQUAL(true, phy->rx_frequencies.empty());
			phy->tuneReceiver(center_freq1);
			CPPUNIT_ASSERT_EQUAL(false, phy->rx_frequencies.empty());
			auto *packet_destined_to_us2 = new L2Packet();
			packet_destined_to_us2->addMessage(new L2HeaderBase(communication_partner_id, 0, 0, 0, 0), nullptr);
			packet_destined_to_us2->addMessage(new L2HeaderUnicast(own_id, false, 0, 0), nullptr);
			phy->onReception(packet_destined_to_us2, center_freq1);
			// Should *not* have been discarded.
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) phy->stat_num_packets_rcvd.get());			
			// And so the number of missed packets doesn't increase.
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) phy->stat_num_packets_missed.get());

			phy->update(1);
			CPPUNIT_ASSERT_EQUAL(true, phy->rx_frequencies.empty());
			phy->onReception(packet_destined_to_us2, center_freq1);			

			// Should again be discarded - no receiver is tuned *in this time slot*.
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) phy->stat_num_packets_rcvd.get());
			CPPUNIT_ASSERT_EQUAL(size_t(2), (size_t) phy->stat_num_packets_missed.get());			            
		}

	CPPUNIT_TEST_SUITE(MCSOTDMA_PhyTests);
		CPPUNIT_TEST(testDiscardPacketWhenNoReceiverListens);
	CPPUNIT_TEST_SUITE_END();
	};
}