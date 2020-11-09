//
// Created by Sebastian Lindner on 09.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../L2Packet.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

class L2PacketTests : public CppUnit::TestFixture {
	private:
		L2Packet* packet;
		
	class TestPayload : public L2Packet::Payload {
		unsigned int getBits() const override {
			return 1;
		}
	};
	
	public:
		void setUp() override {
			packet = new L2Packet();
		}
		
		void tearDown() override {
			delete packet;
		}
		
		void testAddPayload() {
			LinkId id_src = LinkId(42), id_dest = LinkId(43);
			unsigned int offset = 12;
			unsigned short length_current = 13;
			unsigned short length_next = 10;
			unsigned int timeout = 12;
			bool use_arq = true;
			unsigned int arq_seqno = 100;
			unsigned int arq_ack_no = 101;
			unsigned int arq_ack_slot = 102;
			L2HeaderUnicast unicast_header = L2HeaderUnicast(id_dest, use_arq, arq_seqno, arq_ack_no, arq_ack_slot);
			TestPayload payload = TestPayload();
			bool exception_occurred = false;
			// Can't add a non-base header as first header.
			try {
				packet->addPayload(&unicast_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
			
			// Can add a base header.
			L2HeaderBase base_header = L2HeaderBase(id_src, offset, length_current, length_next, timeout);
			exception_occurred = false;
			try {
				packet->addPayload(&base_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_occurred);
			
			// Now can add any other header.
			exception_occurred = false;
			try {
				packet->addPayload(&unicast_header, &payload);
			} catch (const std::exception& e) {
				std::cout << e.what() << std::endl;
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_occurred);
			
			// But not another base header.
			exception_occurred = false;
			try {
				packet->addPayload(&base_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
		}
		
		void testUnicastPayload() {
			LinkId id_src = LinkId(42), id_dest = LinkId(43);
			unsigned int offset = 12;
			unsigned short length_current = 13;
			unsigned short length_next = 10;
			unsigned int timeout = 12;
			bool use_arq = true;
			unsigned int arq_seqno = 100;
			unsigned int arq_ack_no = 101;
			unsigned int arq_ack_slot = 102;
			L2HeaderUnicast unicast_header = L2HeaderUnicast(id_dest, use_arq, arq_seqno, arq_ack_no, arq_ack_slot);
			L2HeaderBase base_header = L2HeaderBase(id_src, offset, length_current, length_next, timeout);
			TestPayload payload = TestPayload();
			
			// Add a base and a unicast header.
			bool exception_occurred = false;
			try {
				packet->addPayload(&base_header, &payload);
				packet->addPayload(&unicast_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_occurred);
			
			// Shouldn't be able to add a unicast header to a different destination.
			LinkId id_dest_2 = LinkId(44);
			L2HeaderUnicast second_unicast_header = L2HeaderUnicast(id_dest_2, use_arq, arq_seqno, arq_ack_no, arq_ack_slot);
			exception_occurred = false;
			try {
				packet->addPayload(&second_unicast_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
		}
		
		void testBroadcastPayload() {
			LinkId id_src = LinkId(42), id_dest = LinkId(43);
			unsigned int offset = 12;
			unsigned short length_current = 13;
			unsigned short length_next = 10;
			unsigned int timeout = 12;
			bool use_arq = true;
			unsigned int arq_seqno = 100;
			unsigned int arq_ack_no = 101;
			unsigned int arq_ack_slot = 102;
			
			L2HeaderBase base_header = L2HeaderBase(id_src, offset, length_current, length_next, timeout);
			L2HeaderBroadcast broadcast_header = L2HeaderBroadcast();
			L2HeaderUnicast unicast_header = L2HeaderUnicast(id_dest, use_arq, arq_seqno, arq_ack_no, arq_ack_slot);
			TestPayload payload = TestPayload();
			
			// Add a base and a broadcast header and then a unicast header.
			bool exception_occurred = false;
			try {
				packet->addPayload(&base_header, &payload);
				packet->addPayload(&broadcast_header, &payload);
				packet->addPayload(&unicast_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_occurred);
			
			
		}
	
	CPPUNIT_TEST_SUITE(L2PacketTests);
		CPPUNIT_TEST(testAddPayload);
		CPPUNIT_TEST(testUnicastPayload);
		CPPUNIT_TEST(testBroadcastPayload);
	CPPUNIT_TEST_SUITE_END();
};