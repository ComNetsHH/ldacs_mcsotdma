//
// Created by Sebastian Lindner on 09.11.20.
//


#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../L2Header.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

class L2HeaderTests : public CppUnit::TestFixture {
	private:
		L2Header* header;
		IcaoId id = IcaoId(42);
		unsigned int offset = 12;
		unsigned short length_current = 13;
		unsigned short length_next = 10;
		unsigned int timeout = 12;
	
	public:
		void setUp() override {
			header = new L2Header();
		}
		
		void tearDown() override {
			delete header;
		}
		
		void testHeader() {
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::unset, header->frame_type);
		}
		
		void testBaseHeader() {
			L2HeaderBase header_base = L2HeaderBase(id, offset, length_current, length_next, timeout);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, header_base.frame_type);
			CPPUNIT_ASSERT(header_base.getId() == id);
			CPPUNIT_ASSERT_EQUAL(offset, header_base.offset);
			CPPUNIT_ASSERT_EQUAL(length_current, header_base.length_current);
			CPPUNIT_ASSERT_EQUAL(length_next, header_base.length_next);
			CPPUNIT_ASSERT_EQUAL(timeout, header_base.timeout);
		}
		
		void testBroadcastHeader() {
			L2HeaderBroadcast header_broadcast = L2HeaderBroadcast();
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::broadcast, header_broadcast.frame_type);
		}
		
		void testUnicastHeader() {
			IcaoId dest_id = IcaoId(99);
			bool use_arq = true;
			unsigned int arq_seqno = 50;
			unsigned int arq_ack_no = 51;
			unsigned int arq_ack_slot = 52;
			L2HeaderUnicast header_unicast = L2HeaderUnicast(dest_id, use_arq, arq_seqno, arq_ack_no, arq_ack_slot);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::unicast, header_unicast.frame_type);
			CPPUNIT_ASSERT_EQUAL(use_arq, header_unicast.use_arq);
			CPPUNIT_ASSERT_EQUAL(arq_seqno, header_unicast.arq_seqno);
			CPPUNIT_ASSERT_EQUAL(arq_ack_no, header_unicast.arq_ack_no);
			CPPUNIT_ASSERT_EQUAL(arq_ack_slot, header_unicast.arq_ack_slot);
		}
		
		void testHeaderSizes() {
			L2HeaderBase base_header = L2HeaderBase(id, offset, length_current, length_next, timeout);
			CPPUNIT_ASSERT_EQUAL(uint(70), base_header.getBits());
			
			IcaoId dest_id = IcaoId(99);
			bool use_arq = true;
			unsigned int arq_seqno = 50;
			unsigned int arq_ack_no = 51;
			unsigned int arq_ack_slot = 52;
			L2HeaderUnicast unicast_header = L2HeaderUnicast(dest_id, use_arq, arq_seqno, arq_ack_no, arq_ack_slot);
			CPPUNIT_ASSERT_EQUAL(uint(71), unicast_header.getBits());
			
			L2HeaderBroadcast broadcast_header = L2HeaderBroadcast();
			CPPUNIT_ASSERT_EQUAL(uint(19), broadcast_header.getBits());
			
			L2Header simple_header = L2Header();
			CPPUNIT_ASSERT(simple_header.getBits() == broadcast_header.getBits());
		}
		
	CPPUNIT_TEST_SUITE(L2HeaderTests);
		CPPUNIT_TEST(testHeader);
		CPPUNIT_TEST(testBaseHeader);
		CPPUNIT_TEST(testBroadcastHeader);
		CPPUNIT_TEST(testUnicastHeader);
		CPPUNIT_TEST(testHeaderSizes);
	CPPUNIT_TEST_SUITE_END();
};