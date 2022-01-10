//
// Created by Sebastian Lindner on 04.12.20.
//

//
// Created by Sebastian Lindner on 04.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../MCSOTDMA_Mac.hpp"
#include "MockLayers.hpp"
#include "../LinkManager.hpp"


namespace TUHH_INTAIRNET_MCSOTDMA {

	class MCSOTDMA_MacTests : public CppUnit::TestFixture {
	private:
		PHYLayer* phy;
		MACLayer* mac;
		ARQLayer* arq;
		RLCLayer* rlc;
		TestEnvironment* env;
		
		MacId partner_id = MacId(42);
		MacId own_id = MacId(41);

	public:
		void setUp() override {
			env = new TestEnvironment(own_id, partner_id, true);
			phy = env->phy_layer;
			mac = env->mac_layer;
			arq = env->arq_layer;
			rlc = env->rlc_layer;						
		}

		void tearDown() override {
			delete env;
		}

		void testLinkManagerCreation() {
//				coutd.setVerbose(true);
			CPPUNIT_ASSERT_EQUAL(size_t(0), mac->link_managers.size());
			MacId id = MacId(42);
			mac->notifyOutgoing(1024, id);
			CPPUNIT_ASSERT_EQUAL(size_t(2), mac->link_managers.size());
			auto *link_manager = (LinkManager*) mac->link_managers.at(id);
			CPPUNIT_ASSERT(link_manager);
			CPPUNIT_ASSERT(id == link_manager->getLinkId());
//				coutd.setVerbose(false);
		}

		void testPositions() {
			// Should be able to get your own position.
			bool exception_thrown = false;
			CPRPosition pos = CPRPosition(1, 2, 3, true);
			CPPUNIT_ASSERT(mac->position_map[mac->id] != pos);
			try {
				pos = mac->getPosition(own_id);
			} catch (const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
			CPPUNIT_ASSERT(mac->position_map[mac->id] == pos);
			// Shouldn't be able to get some other user's position, who we've never heard of.
			try {
				pos = mac->getPosition(partner_id);
			} catch (const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
		}

		void testCollision() {
			auto *packet1 = new L2Packet(), *packet2 = new L2Packet();
			packet1->addMessage(new L2HeaderBase(MacId(10), 0, 0, 0, 0), nullptr);
			packet1->addMessage(new L2HeaderBroadcast(), nullptr);
			packet2->addMessage(new L2HeaderBase(MacId(11), 0, 0, 0, 0), nullptr);
			packet2->addMessage(new L2HeaderBroadcast(), nullptr);
			mac->receiveFromLower(packet1, env->bc_frequency);
			mac->receiveFromLower(packet2, env->bc_frequency);
			mac->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(size_t(2), (size_t) mac->stat_num_packet_collisions.get());			
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_packets_rcvd.get());
		}

		void testChannelError() {
			auto *packet = new L2Packet();
			packet->addMessage(new L2HeaderBase(MacId(10), 0, 0, 0, 0), nullptr);
			packet->addMessage(new L2HeaderBroadcast(), nullptr);
			packet->hasChannelError = true;
			mac->receiveFromLower(packet, env->bc_frequency);
			mac->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_channel_errors.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_packets_rcvd.get());
		}

		void testCollisionAndChannelError() {
			auto *packet1 = new L2Packet(), *packet2 = new L2Packet();
			packet1->addMessage(new L2HeaderBase(MacId(10), 0, 0, 0, 0), nullptr);
			packet1->addMessage(new L2HeaderBroadcast(), nullptr);
			packet1->hasChannelError = true;
			packet2->addMessage(new L2HeaderBase(MacId(11), 0, 0, 0, 0), nullptr);
			packet2->addMessage(new L2HeaderBroadcast(), nullptr);
			mac->receiveFromLower(packet1, env->bc_frequency);
			mac->receiveFromLower(packet2, env->bc_frequency);
			mac->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(size_t(2), (size_t) mac->stat_num_packet_collisions.get());			
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_channel_errors.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_packets_rcvd.get());
		}


		CPPUNIT_TEST_SUITE(MCSOTDMA_MacTests);
			CPPUNIT_TEST(testLinkManagerCreation);
			CPPUNIT_TEST(testPositions);
			CPPUNIT_TEST(testCollision);
			CPPUNIT_TEST(testChannelError);			
			CPPUNIT_TEST(testCollisionAndChannelError);			
		CPPUNIT_TEST_SUITE_END();
	};

}