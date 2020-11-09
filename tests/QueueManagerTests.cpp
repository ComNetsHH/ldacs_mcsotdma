//
// Created by Sebastian Lindner on 09.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../QueueManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

class QueueManagerTests : public CppUnit::TestFixture {
	private:
		class TestPayload : public L2Packet::Payload {
				unsigned int getBits() const override {
					return 1;
				}
		};
		
		QueueManager* queue_manager;
		L2Packet* packet;
		L2Header* header;
		TestPayload* payload;
		LinkId id = LinkId(42);
		unsigned int offset = 10;
		unsigned short length_current = 11, length_next = 12;
		unsigned int timeout = 13;
	
	public:
		void setUp() override {
			uint32_t planning_horizon = 1024;
			queue_manager = new QueueManager();
			packet = new L2Packet();
			header = new L2HeaderBase(id, offset, length_current, length_next, timeout);
			payload = new TestPayload();
			packet->addPayload(header, payload);
		}
		
		void tearDown() override {
			delete queue_manager;
			delete packet;
			delete header;
			delete payload;
		}
		
		void testPushBroadcastPacket() {
			// Shouldn't be able to add a destination-less packet.
			bool exception_occurred = false;
			try {
				queue_manager->push(packet);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
			
			// So add a broadcast header, setting the packet destination.
			L2HeaderBroadcast broadcast_header = L2HeaderBroadcast();
			packet->addPayload(&broadcast_header, payload);
			exception_occurred = false;
			QueueManager::Result result;
			try {
				result = queue_manager->push(packet); // should now work.
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_occurred);
			CPPUNIT_ASSERT_EQUAL(QueueManager::enqueued_bc, result);
		}
		
		void testPushUnicastPackets() {
			// Shouldn't be able to add a destination-less packet.
			bool exception_occurred = false;
			try {
				queue_manager->push(packet);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
			
			// So add a unicast header, setting the packet destination.
			LinkId dest_id = LinkId(100);
			bool use_arq = true;
			unsigned int arq_seqno = 101, arq_ack_no = 102, arq_ack_slot = 103;
			L2HeaderUnicast unicast_header = L2HeaderUnicast(dest_id, use_arq, arq_seqno, arq_ack_no, arq_ack_slot);
			packet->addPayload(&unicast_header, payload);
			exception_occurred = false;
			QueueManager::Result result1, result2;
			try {
				result1 = queue_manager->push(packet); // Should require a new link.
				result2 = queue_manager->push(packet); // Should just be enqueued (no new link required).
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_occurred);
			CPPUNIT_ASSERT_EQUAL(QueueManager::enqueued_new_p2p, result1);
			CPPUNIT_ASSERT_EQUAL(QueueManager::enqueued_p2p, result2);
		}
		
		
	CPPUNIT_TEST_SUITE(QueueManagerTests);
		CPPUNIT_TEST(testPushBroadcastPacket);
		CPPUNIT_TEST(testPushUnicastPackets);
	CPPUNIT_TEST_SUITE_END();
};