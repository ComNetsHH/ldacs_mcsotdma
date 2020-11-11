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
			L2HeaderUnicast unicast_header = L2HeaderUnicast(IcaoId(43), true, 100, 101, 102);
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
			L2HeaderBase base_header = L2HeaderBase(IcaoId(42), 12, 13, 14, 15);
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
			L2HeaderUnicast unicast_header = L2HeaderUnicast(IcaoId(43), true, 100, 101, 102);
			L2HeaderBase base_header = L2HeaderBase(IcaoId(42), 12, 13, 14, 15);
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
			IcaoId id_dest_2 = IcaoId(44);
			L2HeaderUnicast second_unicast_header = L2HeaderUnicast(id_dest_2, true, 100, 101, 102);
			exception_occurred = false;
			try {
				packet->addPayload(&second_unicast_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
			
			// Shouldn't be able to add a broadcast header.
			exception_occurred = false;
			try {
				L2HeaderBroadcast broadcast_header = L2HeaderBroadcast();
				packet->addPayload(&broadcast_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
			
			// Shouldn't be able to add a beacon header.
			exception_occurred = false;
			try {
				L2HeaderBeacon beacon_header = L2HeaderBeacon(CPRPosition(1,2,3), true, 2, 1);
				packet->addPayload(&beacon_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
		}
		
		void testBroadcastPayload() {
			L2HeaderBase base_header = L2HeaderBase(IcaoId(42), 12, 13, 14, 15);
			L2HeaderBroadcast broadcast_header = L2HeaderBroadcast();
			L2HeaderUnicast unicast_header = L2HeaderUnicast(IcaoId(43), true, 100, 101, 102);
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
			
			// Shouldn't be able to add a broadcast header.
			exception_occurred = false;
			try {
				packet->addPayload(&broadcast_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
			
			// Shouldn't be able to add a beacon header.
			exception_occurred = false;
			try {
				L2HeaderBeacon beacon_header = L2HeaderBeacon(CPRPosition(1,2,3), true, 2, 1);
				packet->addPayload(&beacon_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
		}
		
		void testBeaconPayload() {
			L2HeaderBase base_header = L2HeaderBase(IcaoId(42), 12, 13, 14, 15);
			L2HeaderBeacon beacon_header = L2HeaderBeacon(CPRPosition(1, 2, 3), true, 50, 1);
			TestPayload payload = TestPayload();
			// Should be able to add a beacon header.
			bool exception_occurred = false;
			try {
				packet->addPayload(&base_header, &payload);
				packet->addPayload(&beacon_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_occurred);
			
			// Should be able to add a broadcast header.
			exception_occurred = false;
			try {
				L2HeaderBroadcast broadcast_header = L2HeaderBroadcast();
				packet->addPayload(&broadcast_header, &payload);
			} catch (const std::exception& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(false, exception_occurred);
			
			// Should be able to add a unicast header.
			exception_occurred = false;
			try {
				L2HeaderUnicast unicast_header = L2HeaderUnicast(IcaoId(43), true, 100, 101, 102);;
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
		CPPUNIT_TEST(testBeaconPayload);
	CPPUNIT_TEST_SUITE_END();
};