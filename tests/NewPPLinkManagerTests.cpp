//
// Created by Sebastian Lindner on 12/21/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../NewPPLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class NewPPLinkManagerTests : public CppUnit::TestFixture {
	private:
		TestEnvironment *env;
		uint32_t planning_horizon;
		NewPPLinkManager *link_manager;
		MacId own_id, partner_id;		
		ReservationManager *reservation_manager;

	public:
		void setUp() override {
			own_id = MacId(42);
			partner_id = MacId(43);
			env = new TestEnvironment(own_id, partner_id, true);
			link_manager = (NewPPLinkManager*) env->mac_layer->getLinkManager(partner_id);
			reservation_manager = env->mac_layer->getReservationManager();
		}

		void tearDown() override {
			delete env;
		}

		/** When new data is reported and the link is not established, establishment should be triggered. */
		void testStartLinkEstablishment() {
			link_manager->notifyOutgoing(100);
		}

		/** When new data is reported and the link is *not unestablished*, establishment should *not* be triggered. */
		void testDontStartLinkEstablishmentIfNotUnestablished() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
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