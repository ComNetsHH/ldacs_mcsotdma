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
			env = new TestEnvironment(own_id, partner_id);
			phy = env->phy_layer;
			mac = env->mac_layer;
			arq = env->arq_layer;
			rlc = env->rlc_layer;						
		}

		void tearDown() override {
			delete env;
		}

// 		void testLinkManagerCreation() {
// //				coutd.setVerbose(true);
// 			CPPUNIT_ASSERT_EQUAL(size_t(0), mac->link_managers.size());
// 			MacId id = MacId(42);
// 			mac->notifyOutgoing(1024, id);
// 			CPPUNIT_ASSERT_EQUAL(size_t(2), mac->link_managers.size());
// 			auto *link_manager = (LinkManager*) mac->link_managers.at(id);
// 			CPPUNIT_ASSERT(link_manager);
// 			CPPUNIT_ASSERT(id == link_manager->getLinkId());
// //				coutd.setVerbose(false);
// 		}

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
			packet1->addMessage(new L2HeaderSH(MacId(10)), nullptr);			
			packet2->addMessage(new L2HeaderSH(MacId(11)), nullptr);			
			mac->receiveFromLower(packet1, env->sh_frequency);
			mac->receiveFromLower(packet2, env->sh_frequency);
			mac->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_packet_collisions.get());			
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_packets_rcvd.get());
		}

		void testChannelError() {
			auto *packet = new L2Packet();
			packet->addMessage(new L2HeaderSH(MacId(10)), nullptr);			
			packet->hasChannelError = true;
			mac->receiveFromLower(packet, env->sh_frequency);
			mac->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_channel_errors.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_packets_rcvd.get());
		}

		void testCollisionAndChannelError() {
			auto *packet1 = new L2Packet(), *packet2 = new L2Packet();
			packet1->addMessage(new L2HeaderSH(MacId(10)), nullptr);			
			packet1->hasChannelError = true;
			packet2->addMessage(new L2HeaderSH(MacId(11)), nullptr);			
			mac->receiveFromLower(packet1, env->sh_frequency);
			mac->receiveFromLower(packet2, env->sh_frequency);
			mac->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_packet_collisions.get());			
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_channel_errors.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_packets_rcvd.get());
		}

		void testDMEPacketChannelSensing() {
			CPPUNIT_ASSERT_THROW(mac->getChannelSensingObservation(), std::runtime_error);
			mac->setLearnDMEActivity(true);
			CPPUNIT_ASSERT_NO_THROW(mac->getChannelSensingObservation());			
		}		

		CPPUNIT_TEST_SUITE(MCSOTDMA_MacTests);
			CPPUNIT_TEST(testPositions);
			CPPUNIT_TEST(testCollision);
			CPPUNIT_TEST(testChannelError);			
			CPPUNIT_TEST(testCollisionAndChannelError);			
			CPPUNIT_TEST(testDMEPacketChannelSensing);						
		CPPUNIT_TEST_SUITE_END();
	};

}