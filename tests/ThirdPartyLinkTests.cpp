//
// Created by seba on 4/14/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ThirdPartyLink.hpp"
#include "../NewPPLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class ThirdPartyLinkTests : public CppUnit::TestFixture {
	private:
		ThirdPartyLink *link;
		MacId id_initiator, id_recipient, id;
		TestEnvironment *env_initator, *env_recipient, *env;
		MCSOTDMA_Mac *mac, *mac_initiator, *mac_recipient;
		ReservationManager *reservation_manager;
		NewPPLinkManager *pp_initiator, *pp_recipient;

	public:
		void setUp() override {
			id_initiator = MacId(42);
			id_recipient = MacId(43);
			id = MacId(44);
			// these establish links
			env_initator = new TestEnvironment(id_initiator, id_recipient, true);
			mac_initiator = env_initator->mac_layer;
			pp_initiator = (NewPPLinkManager*) mac_initiator->getLinkManager(id_recipient);
			env_recipient = new TestEnvironment(id_recipient, id_initiator, true);
			mac_recipient = env_recipient->mac_layer;
			pp_recipient = (NewPPLinkManager*) mac_recipient->getLinkManager(id_initiator);
			// this will be the third party
			env = new TestEnvironment(id, id_initiator, true);
			mac = env->mac_layer;
			reservation_manager = mac->getReservationManager();			
			link = new ThirdPartyLink(id_initiator, id_recipient, env->mac_layer);
			// connect 'em all
			env_initator->phy_layer->connected_phys.push_back(env_recipient->phy_layer);
			env_initator->phy_layer->connected_phys.push_back(env->phy_layer);
			env_recipient->phy_layer->connected_phys.push_back(env_initator->phy_layer);
			env_recipient->phy_layer->connected_phys.push_back(env->phy_layer);
			env->phy_layer->connected_phys.push_back(env_initator->phy_layer);
			env->phy_layer->connected_phys.push_back(env_recipient->phy_layer);
		}

		void tearDown() override {
			delete link;
			delete env_initator;
			delete env_recipient;
			delete env;
		}

		void testGetThirdPartyLink() {
			auto &link = mac->getThirdPartyLink(id_initiator, id_recipient);
			int some_val = 42;
			link.num_slots_until_expected_link_reply = some_val;
			auto &second_ref = mac->getThirdPartyLink(id_initiator, id_recipient);
			CPPUNIT_ASSERT_EQUAL(link.num_slots_until_expected_link_reply, second_ref.num_slots_until_expected_link_reply);
		}

		/** A link request should lock all links that are proposed. */
		void testLinkRequestLocks() {
			// start link establishment
			pp_initiator->notifyOutgoing(1);
			size_t num_slots = 0, max_slots = 100;
			while (pp_initiator->link_status != LinkManager::awaiting_reply && num_slots++ < max_slots) {
				mac_initiator->update(1);
				mac_recipient->update(1);
				mac->update(1);
				mac_initiator->execute();
				mac_recipient->execute();
				mac->execute();
				mac_initiator->onSlotEnd();
				mac_recipient->onSlotEnd();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_reply, pp_initiator->link_status);
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_requests_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_recipient->stat_num_requests_rcvd.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_thid_party_requests_rcvd.get());
			// the locks at the link initiator and our third party should match
			size_t num_locks_initiator = 0, num_locks_thirdparty = 0;
			for (auto *channel : reservation_manager->getP2PFreqChannels()) {
				const auto *tbl_initiator = env_initator->mac_layer->reservation_manager->getReservationTable(channel);
				const auto *tbl_thirdparty = reservation_manager->getReservationTable(channel);
				for (size_t t = 0; t < env->planning_horizon; t++) {					
					CPPUNIT_ASSERT_EQUAL(tbl_initiator->getReservation(t).getAction(), tbl_thirdparty->getReservation(t).getAction());
					if (tbl_initiator->getReservation(t).isLocked())
						num_locks_initiator++;
					if (tbl_thirdparty->getReservation(t).isLocked())
						num_locks_thirdparty++;
				}
			}
			CPPUNIT_ASSERT_GREATER(size_t(0), num_locks_initiator);
			CPPUNIT_ASSERT_EQUAL(num_locks_initiator, num_locks_thirdparty);
		}		

		/** After locks were made through the processing of a third-party link request, a counter is started that expects a link reply.
		 * If no such reply arrives, all locks should be undone. */
		void testMissingReplyUnlocks() {
			testLinkRequestLocks();
			ThirdPartyLink &link = mac->getThirdPartyLink(id_initiator, id_recipient);
			// both link initiator and the third party user should agree on the slot offset where the link reply is expected
			CPPUNIT_ASSERT_GREATER(0, link.num_slots_until_expected_link_reply);
			CPPUNIT_ASSERT_EQUAL(link.num_slots_until_expected_link_reply, ((NewPPLinkManager*) mac_initiator->getLinkManager(id_recipient))->link_state.reply_offset);
			// drop all packets from now on => link reply will surely not be received
			env->phy_layer->connected_phys.clear();
			env_initator->phy_layer->connected_phys.clear();
			env_recipient->phy_layer->connected_phys.clear();
			// proceed past expected reply slot
			int expected_reply_slot = link.num_slots_until_expected_link_reply;
			for (int t = 0; t < expected_reply_slot; t++) {
				mac_initiator->update(1);
				mac_recipient->update(1);
				mac->update(1);
				mac_initiator->execute();
				mac_recipient->execute();
				mac->execute();
				mac_initiator->onSlotEnd();
				mac_recipient->onSlotEnd();
				mac->onSlotEnd();
			}
			// both link initiator and third party should now have zero locks
			size_t num_locks_initiator = 0, num_locks_thirdparty = 0;
			for (auto *channel : reservation_manager->getP2PFreqChannels()) {
				const auto *tbl_initiator = env_initator->mac_layer->reservation_manager->getReservationTable(channel);
				const auto *tbl_thirdparty = reservation_manager->getReservationTable(channel);
				for (size_t t = 0; t < env->planning_horizon; t++) {					
					CPPUNIT_ASSERT_EQUAL(tbl_initiator->getReservation(t).getAction(), tbl_thirdparty->getReservation(t).getAction());
					if (tbl_initiator->getReservation(t).isLocked())
						num_locks_initiator++;
					if (tbl_thirdparty->getReservation(t).isLocked())
						num_locks_thirdparty++;
				}
			}
			CPPUNIT_ASSERT_EQUAL(size_t(0), num_locks_initiator);
			CPPUNIT_ASSERT_EQUAL(num_locks_initiator, num_locks_thirdparty);
		}	

		CPPUNIT_TEST_SUITE(ThirdPartyLinkTests);		
			CPPUNIT_TEST(testGetThirdPartyLink);			
			CPPUNIT_TEST(testLinkRequestLocks);
			CPPUNIT_TEST(testMissingReplyUnlocks);
		CPPUNIT_TEST_SUITE_END();
	};

}