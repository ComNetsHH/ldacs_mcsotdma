//
// Created by Sebastian Lindner on 12/21/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../NewPPLinkManager.hpp"
#include "../SHLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class NewPPLinkManagerTests : public CppUnit::TestFixture {
	private:
		TestEnvironment *env;
		uint32_t planning_horizon;
		NewPPLinkManager *pp;
		SHLinkManager *sh;
		MacId own_id, partner_id;		
		ReservationManager *reservation_manager;
		MCSOTDMA_Mac *mac;

	public:
		void setUp() override {
			own_id = MacId(42);
			partner_id = MacId(43);
			env = new TestEnvironment(own_id, partner_id, true);
			pp = (NewPPLinkManager*) env->mac_layer->getLinkManager(partner_id);
			sh = (SHLinkManager*) env->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			mac = env->mac_layer;
			reservation_manager = env->mac_layer->getReservationManager();
		}

		void tearDown() override {
			delete env;
		}

		/** When new data is reported and the link is not established, establishment should be triggered. */
		void testStartLinkEstablishment() {
			// initially no link requests and no scheduled broadcast slot
			CPPUNIT_ASSERT_EQUAL(size_t(0), sh->link_requests.size());	
			CPPUNIT_ASSERT_EQUAL(false, sh->next_broadcast_scheduled);		
			// trigger link establishment
			mac->notifyOutgoing(100, partner_id);
			// now there should be a link request
			CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
			// and a scheduled broadcast slot
			CPPUNIT_ASSERT_EQUAL(true, sh->next_broadcast_scheduled);
		}

		/** When new data is reported and the link is *not unestablished*, establishment should *not* be triggered. */
		void testDontStartLinkEstablishmentIfNotUnestablished() {
			// initially no link requests and no scheduled broadcast slot
			CPPUNIT_ASSERT_EQUAL(size_t(0), sh->link_requests.size());	
			CPPUNIT_ASSERT_EQUAL(false, sh->next_broadcast_scheduled);		
			// trigger link establishment
			mac->notifyOutgoing(100, partner_id);
			// now there should be a link request
			CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
			// and a scheduled broadcast slot
			CPPUNIT_ASSERT_EQUAL(true, sh->next_broadcast_scheduled);
			unsigned int broadcast_slot = sh->next_broadcast_slot;
			CPPUNIT_ASSERT_GREATER(uint(0), broadcast_slot);
			
			// now, notify about even more data
			mac->notifyOutgoing(100, partner_id);
			// which shouldn't have changed anything
			CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
			CPPUNIT_ASSERT_EQUAL(true, sh->next_broadcast_scheduled);
			CPPUNIT_ASSERT_EQUAL(broadcast_slot, sh->next_broadcast_slot);
		}

		/** When the link request is being transmitted, this should trigger slot selection. 
		 * The number of proposed resources should match the settings, and these should all be idle. 
		 * Afterwards, they should be locked. */
		void testSlotSelection() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When no reply has been received in the advertised slot, link establishment should be re-triggered. */
		void testReplySlotPassed() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When an expected reply has been received, the link status should reflect that. */
		void testReplyReceived() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When a link request is received, but the indicated reply slot is not suitable, this should trigger link establishment. */
		void testRequestReceivedButReplySlotUnsuitable() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When a link request is received, but none of the proposed resources are suitable, this should trigger link establishment. */
		void testRequestReceivedButProposedResourcesUnsuitable() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When a link request is received, the reply slot is suitable, a proposed resource is suitable, then this should be selected and the reply slot scheduled. */
		void testProcessRequestAndScheduleReply() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}	

		/** When a link reply is received, this should unschedule any own link replies currently scheduled. 
		 * The first reply is handled.
		 */
		void testUnscheduleOwnReplyUponReplyReception() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When a link reply is received, this should establish the link on the indicated resources. */
		void testEstablishLinkUponReplyReception() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When the first burst has been handled, this should be reflected in both users' stati. */
		void testEstablishLinkUponFirstBurst() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}
 


	CPPUNIT_TEST_SUITE(NewPPLinkManagerTests);
		CPPUNIT_TEST(testStartLinkEstablishment);
		CPPUNIT_TEST(testDontStartLinkEstablishmentIfNotUnestablished);
		CPPUNIT_TEST(testSlotSelection);
		CPPUNIT_TEST(testReplySlotPassed);
		CPPUNIT_TEST(testReplyReceived);
		CPPUNIT_TEST(testRequestReceivedButReplySlotUnsuitable);
		CPPUNIT_TEST(testRequestReceivedButProposedResourcesUnsuitable);
		CPPUNIT_TEST(testProcessRequestAndScheduleReply);
		CPPUNIT_TEST(testUnscheduleOwnReplyUponReplyReception);
		CPPUNIT_TEST(testEstablishLinkUponReplyReception);
		CPPUNIT_TEST(testEstablishLinkUponFirstBurst);
	CPPUNIT_TEST_SUITE_END();
	};
}