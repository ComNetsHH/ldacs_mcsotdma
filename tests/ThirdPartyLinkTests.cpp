//
// Created by seba on 4/14/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ThirdPartyLink.hpp"
#include "../PPLinkManager.hpp"
#include "../SHLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class ThirdPartyLinkTests : public CppUnit::TestFixture {
	private:
		ThirdPartyLink *link;
		MacId id_initiator, id_recipient, id;
		TestEnvironment *env_initator, *env_recipient, *env;
		MCSOTDMA_Mac *mac_initiator, *mac_recipient, *mac;
		ReservationManager *reservation_manager;
		PPLinkManager *pp_initiator, *pp_recipient;
		SHLinkManager *sh_initiator, *sh_recipient, *sh;

	public:
		void setUp() override {
			id_initiator = MacId(42);
			id_recipient = MacId(43);
			id = MacId(44);
			// these establish links
			env_initator = new TestEnvironment(id_initiator, id_recipient);
			mac_initiator = env_initator->mac_layer;
			pp_initiator = (PPLinkManager*) mac_initiator->getLinkManager(id_recipient);
			env_recipient = new TestEnvironment(id_recipient, id_initiator);
			mac_recipient = env_recipient->mac_layer;
			pp_recipient = (PPLinkManager*) mac_recipient->getLinkManager(id_initiator);									
			// this will be the third party
			env = new TestEnvironment(id, id_initiator);
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
			// get pointers
			sh_initiator = (SHLinkManager*) mac_initiator->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			sh_recipient = (SHLinkManager*) mac_recipient->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			sh = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			sh->setShouldTransmit(false);
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
			// wait until advertisement has been received
			size_t num_slots = 0, max_slots = 100;									
			while (mac_initiator->stat_num_broadcasts_rcvd.get() < 1.0 && num_slots++ < max_slots) {
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
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_broadcasts_rcvd.get());			

			// start link establishment
			pp_initiator->notifyOutgoing(1);			
			num_slots = 0;
			while (mac_initiator->stat_num_requests_sent.get() < 1.0 && num_slots++ < max_slots) {
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
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_recipient->stat_num_requests_rcvd.get());
			CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac->stat_num_third_party_requests_rcvd.get());
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

		// /** After locks were made through the processing of a third-party link request, a counter is started that expects a link reply.
		//  * If no such reply arrives, all locks should be undone. */
		// void testMissingReplyUnlocks() {
		// 	testLinkRequestLocks();
		// 	ThirdPartyLink &link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// both link initiator and the third party user should agree on the slot offset where the link reply is expected
		// 	CPPUNIT_ASSERT_GREATER(0, link.num_slots_until_expected_link_reply);
		// 	CPPUNIT_ASSERT_EQUAL(link.num_slots_until_expected_link_reply, ((PPLinkManager*) mac_initiator->getLinkManager(id_recipient))->link_state.reply_offset);
		// 	CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::RX), reservation_manager->getBroadcastReservationTable()->getReservation(link.num_slots_until_expected_link_reply));
		// 	CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::RX), env_initator->mac_layer->reservation_manager->getBroadcastReservationTable()->getReservation(link.num_slots_until_expected_link_reply));
		// 	// drop all packets from now on => link reply will surely not be received
		// 	env->phy_layer->connected_phys.clear();
		// 	env_initator->phy_layer->connected_phys.clear();
		// 	env_recipient->phy_layer->connected_phys.clear();
		// 	// proceed past expected reply slot
		// 	int expected_reply_slot = link.num_slots_until_expected_link_reply;
		// 	for (int t = 0; t < expected_reply_slot; t++) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	// both link initiator and third party should now have zero locks
		// 	size_t num_locks_initiator = 0, num_locks_thirdparty = 0;
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {
		// 		const auto *tbl_initiator = env_initator->mac_layer->reservation_manager->getReservationTable(channel);
		// 		const auto *tbl_thirdparty = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) {					
		// 			CPPUNIT_ASSERT_EQUAL(tbl_initiator->getReservation(t).getAction(), tbl_thirdparty->getReservation(t).getAction());
		// 			if (tbl_initiator->getReservation(t).isLocked())
		// 				num_locks_initiator++;
		// 			if (tbl_thirdparty->getReservation(t).isLocked())
		// 				num_locks_thirdparty++;
		// 		}
		// 	}
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), num_locks_initiator);
		// 	CPPUNIT_ASSERT_EQUAL(num_locks_initiator, num_locks_thirdparty);
		// }	

		// /** The reception of an expected reply should undo all previously-made locks and schedule all resources along the link. */
		// void testExpectedReply() {
		// 	// start link establishment
		// 	pp_initiator->notifyOutgoing(1);
		// 	size_t num_slots = 0, max_slots = 100;
		// 	while (pp_initiator->link_status != LinkManager::awaiting_data_tx && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_data_tx, pp_initiator->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_requests_sent.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_replies_rcvd.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_recipient->stat_num_requests_rcvd.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_recipient->stat_num_replies_sent.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_third_party_requests_rcvd.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_third_party_replies_rcvd.get());
		// 	// both link initiator and third party should now have zero locks
		// 	size_t num_locks_initiator = 0, num_locks_thirdparty = 0;
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {
		// 		const auto *tbl_initiator = env_initator->mac_layer->reservation_manager->getReservationTable(channel);
		// 		const auto *tbl_thirdparty = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) {										
		// 			if (tbl_initiator->getReservation(t).isLocked()) {
		// 				num_locks_initiator++;
		// 				CPPUNIT_ASSERT_EQUAL(tbl_initiator->getReservation(t).getAction(), tbl_thirdparty->getReservation(t).getAction());
		// 			}
		// 			if (tbl_thirdparty->getReservation(t).isLocked()) {
		// 				num_locks_thirdparty++;
		// 				CPPUNIT_ASSERT_EQUAL(tbl_initiator->getReservation(t).getAction(), tbl_thirdparty->getReservation(t).getAction());
		// 			}
		// 		}
		// 	}
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), num_locks_initiator);
		// 	CPPUNIT_ASSERT_EQUAL(num_locks_initiator, num_locks_thirdparty);
		// 	// and all users should agree on resource reservations for the entire link
		// 	const auto *tbl_initiator = pp_initiator->current_reservation_table;
		// 	const auto *tbl_recipient = pp_recipient->current_reservation_table;
		// 	const auto *tbl_thirdparty = reservation_manager->getReservationTable(tbl_initiator->getLinkedChannel());
		// 	size_t num_tx_at_initiator = 0, num_tx_at_recipient = 0, num_busy_at_thirdparty = 0;
		// 	for (int t = 0; t < env->planning_horizon; t++) {
		// 		const Reservation &res_initiator = tbl_initiator->getReservation(t),
		// 						  &res_recipient = tbl_recipient->getReservation(t),
		// 						  &res_thirdparty = tbl_thirdparty->getReservation(t);
		// 		if (res_initiator.isTx()) {					
		// 			num_tx_at_initiator++;
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::RX), res_recipient);
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::BUSY), res_thirdparty);					
		// 		}
		// 		if (res_recipient.isTx()) {
		// 			num_tx_at_recipient++;
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::RX), res_initiator);
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::BUSY), res_thirdparty);					
		// 		}
		// 		if (res_thirdparty.isBusy())
		// 			num_busy_at_thirdparty++;
		// 	}
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), num_tx_at_initiator);
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), num_tx_at_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(num_tx_at_initiator + num_tx_at_recipient, num_busy_at_thirdparty);
		// }

		// void testUnscheduleAfterTimeHasPassed() {
		// 	// establish link
		// 	pp_initiator->notifyOutgoing(1);
		// 	size_t num_slots = 0, max_slots = 100;
		// 	while (pp_initiator->link_status != LinkManager::link_established && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_initiator->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_recipient->link_status);
		// 	// continue for a couple of time slots
		// 	int max_t = 42;
		// 	for (int t = 0; t < max_t; t++) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	ThirdPartyLink &link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	CPPUNIT_ASSERT_NO_THROW(link.reset());
		// 	// now neither locks nor scheduled resources should exist			
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *tbl = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) {															
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), tbl->getReservation(t));
		// 		}
		// 	}			
		// }

		// /** Ensures that resource reservations match among the three users all the way until link termination. */
		// void testResourceAgreementsMatchOverDurationOfOneLink() {
		// 	env_initator->rlc_layer->should_there_be_more_p2p_data = false;
		// 	env_recipient->rlc_layer->should_there_be_more_p2p_data = false;			
		// 	pp_initiator->notifyOutgoing(1);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_request_generation, pp_initiator->link_status);
		// 	size_t num_slots = 0, max_slots = 1000;
		// 	// proceed until link has been established at both sides
		// 	while (!(pp_initiator->link_status == LinkManager::link_established && pp_recipient->link_status == LinkManager::link_established) && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_initiator->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_recipient->link_status);
		// 	// now proceed until link expiry			
		// 	num_slots = 0;
		// 	while (!(pp_initiator->link_status == LinkManager::link_not_established && pp_recipient->link_status == LinkManager::link_not_established) && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 		// after each slot, the resource reservations should match among the three users
		// 		const auto *tbl_initiator = pp_initiator->current_reservation_table;
		// 		const auto *tbl_recipient = pp_recipient->current_reservation_table;
		// 		if (tbl_initiator != nullptr && tbl_recipient != nullptr) {
		// 			const auto *tbl_thirdparty = reservation_manager->getReservationTable(tbl_initiator->getLinkedChannel());				
		// 			size_t num_tx_at_initiator = 0, num_tx_at_recipient = 0, num_busy_at_thirdparty = 0;
		// 			for (int t = 0; t < env->planning_horizon; t++) {
		// 				const Reservation &res_initiator = tbl_initiator->getReservation(t),
		// 								&res_recipient = tbl_recipient->getReservation(t),
		// 								&res_thirdparty = tbl_thirdparty->getReservation(t);
		// 				if (res_initiator.isTx()) {					
		// 					num_tx_at_initiator++;
		// 					CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::RX), res_recipient);
		// 					CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::BUSY), res_thirdparty);					
		// 				}
		// 				if (res_recipient.isTx()) {
		// 					num_tx_at_recipient++;
		// 					CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::RX), res_initiator);
		// 					CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::BUSY), res_thirdparty);					
		// 				}
		// 				if (res_thirdparty.isBusy())
		// 					num_busy_at_thirdparty++;
		// 			}
		// 			CPPUNIT_ASSERT_GREATER(size_t(0), num_tx_at_initiator);
		// 			CPPUNIT_ASSERT_GREATER(size_t(0), num_tx_at_recipient);
		// 			CPPUNIT_ASSERT_EQUAL(num_tx_at_initiator + num_tx_at_recipient, num_busy_at_thirdparty);
		// 		}
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp_initiator->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp_recipient->link_status);
		// 	ThirdPartyLink &link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), link.locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), link.locked_resources_for_recipient.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), link.scheduled_resources.size());
		// 	// both link initiator and third party should now have zero locks
		// 	size_t num_locks_initiator = 0, num_locks_thirdparty = 0;
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {
		// 		const auto *tbl_initiator = env_initator->mac_layer->reservation_manager->getReservationTable(channel);
		// 		const auto *tbl_thirdparty = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) {										
		// 			if (tbl_initiator->getReservation(t).isLocked()) {
		// 				num_locks_initiator++;
		// 				CPPUNIT_ASSERT_EQUAL(Reservation::LOCKED, tbl_thirdparty->getReservation(t).getAction());
		// 			} 
		// 			if (tbl_thirdparty->getReservation(t).isLocked()) {
		// 				num_locks_thirdparty++;
		// 				CPPUNIT_ASSERT_EQUAL(Reservation::LOCKED, tbl_initiator->getReservation(t).getAction());
		// 			}
		// 		}
		// 	}
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), num_locks_initiator);
		// 	CPPUNIT_ASSERT_EQUAL(num_locks_initiator, num_locks_thirdparty);
		// }

		// void testNoLocksAfterLinkExpiry() {
		// 	env_initator->rlc_layer->should_there_be_more_p2p_data = false;
		// 	env_recipient->rlc_layer->should_there_be_more_p2p_data = false;			
		// 	pp_initiator->notifyOutgoing(1);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_request_generation, pp_initiator->link_status);
		// 	size_t num_slots = 0, max_slots = 1000;
		// 	// proceed until link has terminated on both sides
		// 	while (!(pp_initiator->link_status == LinkManager::link_not_established && pp_recipient->link_status == LinkManager::link_not_established) && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp_initiator->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp_recipient->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_pp_links_established.get());
		// 	ThirdPartyLink &link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), link.locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), link.locked_resources_for_recipient.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), link.scheduled_resources.size());
		// 	// proceed one slot further
		// 	mac_initiator->update(1);
		// 	mac_recipient->update(1);
		// 	mac->update(1);
		// 	mac_initiator->execute();
		// 	mac_recipient->execute();
		// 	mac->execute();
		// 	mac_initiator->onSlotEnd();
		// 	mac_recipient->onSlotEnd();
		// 	mac->onSlotEnd();
		// 	// should now have zero locks
		// 	size_t num_locks = 0;
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *tbl_thirdparty = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) {
		// 			const auto &res = tbl_thirdparty->getReservation(t);
		// 			if (!res.isIdle())
		// 				coutd << "problematic res: " << res << " at t=" << t << std::endl;
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res);									
		// 		}
		// 	}			
		// }

		// void testLinkReestablishment() {
		// 	env_initator->rlc_layer->should_there_be_more_p2p_data = false;
		// 	env_recipient->rlc_layer->should_there_be_more_p2p_data = false;			
		// 	pp_initiator->notifyOutgoing(1);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_request_generation, pp_initiator->link_status);
		// 	size_t num_slots = 0, max_slots = 1000;
		// 	// proceed until link has terminated on both sides
		// 	while (!(pp_initiator->link_status == LinkManager::link_not_established && pp_recipient->link_status == LinkManager::link_not_established) && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp_initiator->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp_recipient->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_pp_links_established.get());
		// 	ThirdPartyLink &link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), link.locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), link.locked_resources_for_recipient.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), link.scheduled_resources.size());
		// 	// both link initiator and third party should now have zero locks
		// 	size_t num_locks_initiator = 0, num_locks_thirdparty = 0;
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {
		// 		const auto *tbl_initiator = env_initator->mac_layer->reservation_manager->getReservationTable(channel);
		// 		const auto *tbl_thirdparty = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) {										
		// 			if (tbl_initiator->getReservation(t).isLocked()) {
		// 				num_locks_initiator++;
		// 				CPPUNIT_ASSERT_EQUAL(Reservation::LOCKED, tbl_thirdparty->getReservation(t).getAction());
		// 			} 
		// 			if (tbl_thirdparty->getReservation(t).isLocked()) {
		// 				num_locks_thirdparty++;
		// 				CPPUNIT_ASSERT_EQUAL(Reservation::LOCKED, tbl_initiator->getReservation(t).getAction());
		// 			}
		// 		}
		// 	}
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), num_locks_initiator);
		// 	CPPUNIT_ASSERT_EQUAL(num_locks_initiator, num_locks_thirdparty);
		// 	// now establish a new link
		// 	pp_initiator->notifyOutgoing(1);
		// 	num_slots = 0;
		// 	while (!(pp_initiator->link_status == LinkManager::link_established && pp_recipient->link_status == LinkManager::link_established) && num_slots++ < max_slots) {
		// 		try {
		// 			mac_initiator->update(1);
		// 			mac_recipient->update(1);
		// 			mac->update(1);
		// 		} catch (const std::exception &e) {
		// 			throw std::runtime_error("error updating at t=" + std::to_string(num_slots) + ": " + std::string(e.what()));
		// 		}
		// 		try {
		// 			mac_initiator->execute();
		// 			mac_recipient->execute();
		// 			mac->execute();
		// 		} catch (const std::exception &e) {
		// 			throw std::runtime_error("error executing at t=" + std::to_string(num_slots) + ": " + std::string(e.what()));
		// 		}
		// 		try {
		// 			mac_initiator->onSlotEnd();
		// 			mac_recipient->onSlotEnd();
		// 			mac->onSlotEnd();
		// 		} catch (const std::exception &e) {
		// 			throw std::runtime_error("error onSlotEnd at t=" + std::to_string(num_slots) + ": " + std::string(e.what()));
		// 		}
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_initiator->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_recipient->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(2), (size_t) mac_initiator->stat_num_pp_links_established.get());
		// }

		// /** Due to the hidden node problem, one user may receive several link requests that want to occupy the same resources. */
		// void testTwoLinkRequestsWithSameResources() {			
		// 	pp_initiator->notifyOutgoing(1);
		// 	size_t num_slots = 0, max_slots = 100;
		// 	while(mac_initiator->stat_num_requests_sent.get() < 1 && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_requests_sent.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_recipient->stat_num_requests_rcvd.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_third_party_requests_rcvd.get());
		// 	// now that one request has been received
		// 	// create another one for another link
		// 	// but with the same resources
		// 	L2Packet *request_packet = nullptr;
		// 	for (auto *packet : env_initator->phy_layer->outgoing_packets) {
		// 		if (packet->getRequestIndex() != -1) {
		// 			request_packet = packet;
		// 			break;
		// 		}
		// 	}
		// 	CPPUNIT_ASSERT(request_packet != nullptr);			
		// 	L2Packet *another_request_packet = request_packet->copy();
		// 	L2HeaderBase *base_header = (L2HeaderBase*) another_request_packet->getHeaders().at(0);
		// 	MacId imaginary_src_id = MacId(id.getId() + 1);
		// 	MacId imaginary_dest_id = MacId(id.getId() + 2);
		// 	base_header->src_id = imaginary_src_id;
		// 	auto *request_header = (L2HeaderLinkRequest*) another_request_packet->getHeaders().at(another_request_packet->getRequestIndex());
		// 	request_header->dest_id = imaginary_dest_id;
		// 	// make sure the 2nd reply is later than the first
		// 	int reply_slot_1 = -1, reply_slot_2;
		// 	auto *&proposed_resources = (LinkManager::LinkEstablishmentPayload*&) another_request_packet->getPayloads().at(another_request_packet->getRequestIndex());						
		// 	for (auto &pair : proposed_resources->resources) {
		// 		const auto *channel = pair.first;
		// 		if (channel->isSH()) {					
		// 			reply_slot_1 = pair.second.at(0);
		// 			reply_slot_2 = reply_slot_1 + 1;					
		// 			proposed_resources->resources.at(channel).at(0) = reply_slot_2;
		// 			break;
		// 		}
		// 	}
		// 	request_header->reply_offset = reply_slot_2;
		// 	CPPUNIT_ASSERT_GREATER(-1, reply_slot_1);						
		// 	mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onPacketReception(another_request_packet);			
		// 	auto &third_party_link_1 = mac->getThirdPartyLink(id_initiator, id_recipient), &third_party_link_2 = mac->getThirdPartyLink(imaginary_src_id, imaginary_dest_id);
		// 	// locks should've been made for first link
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link_1.locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link_1.locked_resources_for_recipient.size());
		// 	// but not for 2nd link
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link_2.locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link_2.locked_resources_for_recipient.size());
		// 	// remember which slots were locked
		// 	std::map<const FrequencyChannel*, std::vector<int>> locked_res;
		// 	size_t num_locks = 0;
		// 	for (const auto *channel : reservation_manager->getP2PFreqChannels()) {
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (int t = 0; t < env->planning_horizon; t++) {
		// 			if (table->isLocked(t)) {
		// 				locked_res[channel].push_back(t);
		// 				num_locks++;
		// 			}
		// 		}
		// 	}	
		// 	CPPUNIT_ASSERT_EQUAL(third_party_link_1.locked_resources_for_initiator.size() + third_party_link_1.locked_resources_for_recipient.size(), num_locks);			
		// 	// proceed until first reply is expected
		// 	// but make sure it's not received
		// 	env_recipient->phy_layer->connected_phys.clear();
		// 	for (int t = 0; t < reply_slot_1; t++) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	// locks should've been undone
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link_1.locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link_1.locked_resources_for_recipient.size());
		// 	// and made for the 2nd link, whose reply is still expected
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link_2.locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link_2.locked_resources_for_recipient.size());
		// 	// make sure it's the same slots (normalized to current time)			
		// 	for (const auto &pair : locked_res) {
		// 		const auto *channel = pair.first;
		// 		const auto &slots = pair.second;
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (int t : slots) {
		// 			int normalized_offset = t - reply_slot_1;
		// 			const auto &res = table->getReservation(normalized_offset);					
		// 			CPPUNIT_ASSERT_EQUAL(true, res.isLocked());
		// 		}				
		// 	}	
		// 	size_t num_locks_now = 0;
		// 	for (const auto *channel : reservation_manager->getP2PFreqChannels()) {
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (int t = 0; t < env->planning_horizon; t++) {
		// 			if (table->isLocked(t)) 						
		// 				num_locks_now++;					
		// 		}
		// 	}
		// 	CPPUNIT_ASSERT_EQUAL(num_locks, num_locks_now);
		// }				

		// /** Tests that all locks in the current or later time slots are unlocked through the reset() function. */
		// void testImmediateResetUnlocks() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until request has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_request_awaiting_reply && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_request_awaiting_reply, third_party_link.status);
		// 	// immediately reset			
		// 	third_party_link.reset();
		// 	// make sure that no locks are still there			
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) 					
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table->getReservation(t));
		// 	}						
		// }

		// /** Tests that all locks in the current or later time slots are unlocked through the reset() function. */
		// void testResetJustBeforeReplyUnlocks() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until request has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_request_awaiting_reply && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_request_awaiting_reply, third_party_link.status);
		// 	// proceed to just before the expected reply
		// 	int num_slots_until_reset = third_party_link.reply_offset;
		// 	CPPUNIT_ASSERT_GREATER(0, num_slots_until_reset);
		// 	for (int t = 0; t < num_slots_until_reset; t++) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	// reset 
		// 	third_party_link.reset();
		// 	// make sure that no locks are still there			
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) 					
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table->getReservation(t));
		// 	}						
		// }

		// /** Tests that all resource reservations in the current or later time slots are unscheduled through the reset() function when it is called just at link establishment. */
		// void testImmediateResetUnschedules() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until reply has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_reply_link_established && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_reply_link_established, third_party_link.status);
		// 	// immediately reset			
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link.scheduled_resources.size());
		// 	third_party_link.reset();
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link.scheduled_resources.size());
		// 	// make sure that no reservations are still there			
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) 					
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table->getReservation(t));
		// 	}	
		// }

		// /** Tests that all resource reservations in the current or later time slots are unscheduled through the reset() function when it is called some time after link establishment but before termination. */
		// void testIntermediateResetUnschedules() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until reply has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_reply_link_established && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_reply_link_established, third_party_link.status);
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link.scheduled_resources.size());
		// 	// proceed until about half the slots until expiry have passed
		// 	int time_to_proceed_to = third_party_link.link_expiry_offset / 2;
		// 	CPPUNIT_ASSERT_GREATER(0, time_to_proceed_to);
		// 	CPPUNIT_ASSERT_LESS(third_party_link.link_expiry_offset, time_to_proceed_to);
		// 	for (int t = 0; t < time_to_proceed_to; t++) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	// reset
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link.scheduled_resources.size());
		// 	third_party_link.reset();
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link.scheduled_resources.size());
		// 	// make sure that no reservations are still there			
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) 					
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table->getReservation(t));
		// 	}						
		// }

		// /** Tests that all resource reservations in the current or later time slots are unscheduled through the reset() function when it is called just before termination. */
		// void testLateResetUnschedules() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until reply has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_reply_link_established && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_reply_link_established, third_party_link.status);
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link.scheduled_resources.size());
		// 	// proceed until just before expiry
		// 	int time_to_proceed_to = third_party_link.link_expiry_offset - 1;
		// 	CPPUNIT_ASSERT_GREATER(0, time_to_proceed_to);
		// 	CPPUNIT_ASSERT_LESS(third_party_link.link_expiry_offset, time_to_proceed_to);
		// 	for (int t = 0; t < time_to_proceed_to; t++) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	// reset
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link.scheduled_resources.size());
		// 	third_party_link.reset();
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link.scheduled_resources.size());
		// 	// make sure that no reservations are still there			
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) {			
		// 			if (!table->getReservation(t).isIdle())
		// 				std::cout << "expected idle but t=" << t << ": " << table->getReservation(t) << std::endl;
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table->getReservation(t));
		// 		}
		// 	}		
		// }

		// /** Tests that upon a request reception, resources are locked, but non-idle resources are not touched. */
		// void testRequestLocksWherePossible() {
		// 	// check which slots will be proposed
		// 	auto map = pp_initiator->slotSelection(pp_initiator->proposal_num_frequency_channels, pp_initiator->proposal_num_time_slots, 2, 1, pp_initiator->burst_offset);
		// 	// cherry-pick first proposed channel
		// 	auto it = map.begin();
		// 	auto cherry_picked_channel = (*it).first;
		// 	auto slots = (*it).second;
		// 	// schedule these slots for sth
		// 	auto table = reservation_manager->getReservationTable(cherry_picked_channel);
		// 	MacId some_other_id = MacId(id.getId() + 42);
		// 	for (auto slot : slots)
		// 		table->mark(slot, Reservation(some_other_id, Reservation::RX));
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until request has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_request_awaiting_reply && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_request_awaiting_reply, third_party_link.status);			
		// 	// make sure that all locks are set except for the channel where another reservation is already present
		// 	size_t num_locks = 0;
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) {
		// 			if (channel == cherry_picked_channel)
		// 				CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table->getReservation(t));
		// 			else if (table->getReservation(t).isLocked())
		// 				num_locks++;
		// 		}					
		// 	}
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), num_locks);
		// }

		// /** Tests that upon a request reception, the expected reply slot is scheduled. */
		// void testRequestSchedulesExpectedReply() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until request has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_request_awaiting_reply && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_request_awaiting_reply, third_party_link.status);		
		// 	// check that indicated reply slot has been scheduled
		// 	auto request_packet = env_initator->phy_layer->outgoing_packets.at(env_initator->phy_layer->outgoing_packets.size() - 1);
		// 	CPPUNIT_ASSERT_GREATER(-1, request_packet->getRequestIndex());
		// 	auto request_header = (L2HeaderLinkRequest*) request_packet->getHeaders().at(request_packet->getRequestIndex());
		// 	int reply_offset = request_header->reply_offset;
		// 	CPPUNIT_ASSERT_GREATER(0, reply_offset);
		// 	CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::RX), sh->current_reservation_table->getReservation(reply_offset));
		// }

		// void testLinkRequestOverwritesBroadcast() {			
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto *sh_recipient = (SHLinkManager*) mac_recipient->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
		// 	while (sh_recipient->link_replies.empty() && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), sh_recipient->link_replies.size());			
		// 	int scheduled_link_reply_slot = sh_recipient->link_replies.at(0).first + 1; 			
		// 	CPPUNIT_ASSERT_GREATER(0, scheduled_link_reply_slot);			
		// 	CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX), sh_recipient->current_reservation_table->getReservation(scheduled_link_reply_slot));
		// 	// now that recipient has scheduled its link reply
		// 	// prepare a 3rd party link request that will overwrite this reservation
		// 	mac->notifyOutgoing(1, id_initiator);
		// 	env->phy_layer->connected_phys.clear();
		// 	num_slots = 0;
		// 	while (size_t(mac->stat_num_requests_sent.get()) < 1 && num_slots++ < max_slots) {
		// 		mac->update(1);
		// 		mac->execute();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_requests_sent.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), sh_recipient->link_replies.size());		
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_replies_sent.get());
		// 	auto *link_request = env->phy_layer->outgoing_packets.at(env->phy_layer->outgoing_packets.size() - 1);
		// 	CPPUNIT_ASSERT_GREATER(-1, link_request->getRequestIndex());
		// 	auto *link_request_header = (L2HeaderLinkRequest*) link_request->getHeaders().at(link_request->getRequestIndex());
		// 	// set to the same slot
		// 	link_request_header->reply_offset = scheduled_link_reply_slot;
		// 	// now receive this 
		// 	mac_recipient->receiveFromLower(link_request->copy(), env->sh_frequency);
		// 	mac_recipient->onSlotEnd();
		// 	// broadcast slot should've been re-scheduled
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_recipient->stat_num_broadcast_collisions_detected.get());						
		// 	CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::RX), sh_recipient->current_reservation_table->getReservation(scheduled_link_reply_slot));
		// 	sh_recipient->unscheduleBroadcastSlot(); // eliminate possible cause for collision
		// 	// so now when the supposed reply slot arrives, nothing will be sent
		// 	for (int t = 0; t < scheduled_link_reply_slot; t++) {
		// 		mac_recipient->update(1);
		// 		mac_recipient->execute();
		// 		mac_recipient->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), sh_recipient->link_replies.size());					
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_replies_sent.get());
		// 	// which should've re-triggered link establishment
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_request_generation, pp_recipient->link_status);
		// }

		// void testLinkRequestOverwritesBeacon() {			
		// 	// progress one slot
		// 	mac_initiator->update(1);
		// 	mac_recipient->update(1);
		// 	mac->update(1);
		// 	mac_initiator->execute();
		// 	mac_recipient->execute();
		// 	mac->execute();
		// 	mac_initiator->onSlotEnd();
		// 	mac_recipient->onSlotEnd();
		// 	mac->onSlotEnd();	
		// 	// beacon should've been scheduled
		// 	auto *sh = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);	
		// 	CPPUNIT_ASSERT_EQUAL(true, sh->next_beacon_scheduled);
		// 	CPPUNIT_ASSERT_GREATER(uint(0), sh->getNextBeaconSlot());					
		// 	CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON), sh->current_reservation_table->getReservation(sh->getNextBeaconSlot()));
		// 	// prepare a 3rd party link request that will overwrite this reservation
		// 	env_initator->phy_layer->connected_phys.clear();
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	while (size_t(mac_initiator->stat_num_requests_sent.get()) < 1 && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_initiator->execute();
		// 		mac_initiator->onSlotEnd();
		// 	}			
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);			
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_requests_sent.get());			
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), env_initator->phy_layer->outgoing_packets.size());			
		// 	auto *link_request = env_initator->phy_layer->outgoing_packets.at(env_initator->phy_layer->outgoing_packets.size() - 1);
		// 	CPPUNIT_ASSERT_GREATER(-1, link_request->getRequestIndex());
		// 	auto *link_request_header = (L2HeaderLinkRequest*) link_request->getHeaders().at(link_request->getRequestIndex());
		// 	// set to the same slot
		// 	unsigned int beacon_slot_before = sh->getNextBeaconSlot();			
		// 	link_request_header->reply_offset = beacon_slot_before;
		// 	// now receive this 
		// 	mac->receiveFromLower(link_request->copy(), env->sh_frequency);
		// 	mac->onSlotEnd();
		// 	// beacon slot should've been re-scheduled
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_beacon_collisions_detected.get());						
		// 	CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::RX), sh->current_reservation_table->getReservation(beacon_slot_before));
		// 	CPPUNIT_ASSERT(beacon_slot_before != sh->getNextBeaconSlot());			
		// }

		// /** Tests that upon reply reception, all locks made after request reception are unlocked. */
		// void testReplyUnlocks() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until request has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_reply_link_established && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_reply_link_established, third_party_link.status);		
		// 	// make sure that no locks are present
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) 
		// 			CPPUNIT_ASSERT_EQUAL(false, table->getReservation(t).isLocked());									
		// 	}
		// }	

		// /** Tests that if an unexpected reply is received, after *no* request had indicated this reply, the link reservations are made correctly. */
		// void testUnexpectedReply() {
		// 	// sever connection between initiator and user-in-question
		// 	env_initator->phy_layer->connected_phys.clear();
		// 	env_initator->phy_layer->connected_phys.push_back(env_recipient->phy_layer);
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	while (pp_initiator->link_status != LinkManager::Status::awaiting_reply && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, pp_initiator->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_requests_sent.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_recipient->stat_num_requests_rcvd.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_third_party_requests_rcvd.get());
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::uninitialized, third_party_link.status);
		// 	// proceed until reply is sent
		// 	num_slots = 0;
		// 	while (mac_recipient->stat_num_replies_sent.get() < 1 && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	// make sure that even though the request had not been received
		// 	// the third party link has the right information
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_reply_link_established, third_party_link.status);
		// 	CPPUNIT_ASSERT_EQUAL(id_initiator, third_party_link.id_link_initiator);
		// 	CPPUNIT_ASSERT_EQUAL(id_recipient, third_party_link.id_link_recipient);
		// 	// and that the correct reservations have been made			
		// 	const auto *tbl_initiator = pp_initiator->current_reservation_table;
		// 	const auto *tbl_recipient = pp_recipient->current_reservation_table;			
		// 	CPPUNIT_ASSERT(tbl_initiator != nullptr && tbl_recipient != nullptr);
		// 	const auto *tbl_thirdparty = reservation_manager->getReservationTable(tbl_initiator->getLinkedChannel());				
		// 	CPPUNIT_ASSERT(tbl_thirdparty != nullptr);
		// 	size_t num_tx_at_initiator = 0, num_tx_at_recipient = 0, num_busy_at_thirdparty = 0;
		// 	for (int t = 0; t < env->planning_horizon; t++) {
		// 		const Reservation &res_initiator = tbl_initiator->getReservation(t),
		// 						&res_recipient = tbl_recipient->getReservation(t),
		// 						&res_thirdparty = tbl_thirdparty->getReservation(t);				
		// 		if (res_initiator.isTx()) {					
		// 			num_tx_at_initiator++;
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::RX), res_recipient);
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::BUSY), res_thirdparty);					
		// 		}
		// 		if (res_recipient.isTx()) {
		// 			num_tx_at_recipient++;
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::RX), res_initiator);
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::BUSY), res_thirdparty);					
		// 		}
		// 		if (res_thirdparty.isBusy())
		// 			num_busy_at_thirdparty++;
		// 	}
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), num_tx_at_initiator);
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), num_tx_at_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(num_tx_at_initiator + num_tx_at_recipient, num_busy_at_thirdparty);			
		// }

		// /** Tests that upon reply reception, all link info is saved. */
		// void testRequestAndReplySaveLinkInfo() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	size_t num_slots = 0, max_slots = 30;
		// 	while (third_party_link.status != ThirdPartyLink::received_request_awaiting_reply && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_request_awaiting_reply, third_party_link.status);
		// 	// after receiving the request, all info should already be there
		// 	CPPUNIT_ASSERT_EQUAL(id_initiator, third_party_link.id_link_initiator);
		// 	CPPUNIT_ASSERT_EQUAL(id_recipient, third_party_link.id_link_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.burst_length, (unsigned int) third_party_link.link_description.burst_length);
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.burst_length_tx, (unsigned int) third_party_link.link_description.burst_length_tx);
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.burst_length_rx, (unsigned int) third_party_link.link_description.burst_length_rx);
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.timeout, (unsigned int) third_party_link.link_description.timeout);			
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.burst_offset, (unsigned int) third_party_link.link_description.burst_offset);
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.burst_length, (unsigned int) third_party_link.link_description.burst_length);
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.burst_length_tx, (unsigned int) third_party_link.link_description.burst_length_tx);
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.burst_length_rx, (unsigned int) third_party_link.link_description.burst_length_rx);
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.timeout, (unsigned int) third_party_link.link_description.timeout);			
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.burst_offset, (unsigned int) third_party_link.link_description.burst_offset);
		// 	// proceed to after reply
		// 	num_slots = 0;
		// 	while (third_party_link.status != ThirdPartyLink::received_reply_link_established && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_reply_link_established, third_party_link.status);
		// 	// and info should still be there
		// 	CPPUNIT_ASSERT_EQUAL(id_initiator, third_party_link.id_link_initiator);
		// 	CPPUNIT_ASSERT_EQUAL(id_recipient, third_party_link.id_link_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.burst_length, (unsigned int) third_party_link.link_description.burst_length);
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.burst_length_tx, (unsigned int) third_party_link.link_description.burst_length_tx);
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.burst_length_rx, (unsigned int) third_party_link.link_description.burst_length_rx);
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.timeout, (unsigned int) third_party_link.link_description.timeout);			
		// 	CPPUNIT_ASSERT_EQUAL(pp_initiator->link_state.burst_offset, (unsigned int) third_party_link.link_description.burst_offset);
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.burst_length, (unsigned int) third_party_link.link_description.burst_length);
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.burst_length_tx, (unsigned int) third_party_link.link_description.burst_length_tx);
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.burst_length_rx, (unsigned int) third_party_link.link_description.burst_length_rx);
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.timeout, (unsigned int) third_party_link.link_description.timeout);			
		// 	CPPUNIT_ASSERT_EQUAL(pp_recipient->link_state.burst_offset, (unsigned int) third_party_link.link_description.burst_offset);
		// }

		// /** Tests that upon reply reception, the link's resource reservations are made. */
		// void testReplySchedulesBursts() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	size_t num_slots = 0, max_slots = 30;
		// 	while (third_party_link.status != ThirdPartyLink::received_reply_link_established && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_reply_link_established, third_party_link.status);
		// 	// check that the correct reservations have been made			
		// 	const auto *tbl_initiator = pp_initiator->current_reservation_table;
		// 	const auto *tbl_recipient = pp_recipient->current_reservation_table;			
		// 	CPPUNIT_ASSERT(tbl_initiator != nullptr && tbl_recipient != nullptr);
		// 	const auto *tbl_thirdparty = reservation_manager->getReservationTable(tbl_initiator->getLinkedChannel());				
		// 	CPPUNIT_ASSERT(tbl_thirdparty != nullptr);
		// 	size_t num_tx_at_initiator = 0, num_tx_at_recipient = 0, num_busy_at_thirdparty = 0;
		// 	for (int t = 0; t < env->planning_horizon; t++) {
		// 		const Reservation &res_initiator = tbl_initiator->getReservation(t),
		// 						&res_recipient = tbl_recipient->getReservation(t),
		// 						&res_thirdparty = tbl_thirdparty->getReservation(t);				
		// 		if (res_initiator.isTx()) {					
		// 			num_tx_at_initiator++;
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::RX), res_recipient);
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::BUSY), res_thirdparty);					
		// 		}
		// 		if (res_recipient.isTx()) {
		// 			num_tx_at_recipient++;
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::RX), res_initiator);
		// 			CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::BUSY), res_thirdparty);					
		// 		}
		// 		if (res_thirdparty.isBusy())
		// 			num_busy_at_thirdparty++;
		// 	}
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), num_tx_at_initiator);
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), num_tx_at_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(num_tx_at_initiator + num_tx_at_recipient, num_busy_at_thirdparty);			
		// }

		// /** Tests that upon reply reception, the link's resource reservations are made, not touching non-idle resources. */
		// void testReplySchedulesBurstsButDoesNotOverwrite() {
		// 	// check which slots will be proposed
		// 	// pp_initiator->setBurstOffset(pp_initiator->computeBurstOffset(pp_initiator->getBurstLength(), 0, reservation_manager->getP2PFreqChannels().size()));
		// 	auto map = pp_initiator->slotSelection(pp_initiator->proposal_num_frequency_channels, pp_initiator->proposal_num_time_slots, 2, 1, pp_initiator->getBurstOffset());
		// 	// schedule initial burst for all channels
		// 	MacId some_other_id = MacId(id.getId() + 42);			
		// 	size_t num_blocked = 0;
		// 	int latest_slot = 0;
		// 	for (const auto &pair : map) {
		// 		auto channel = pair.first;
		// 		if (channel->isSH())
		// 			continue;
		// 		auto slots = pair.second;
		// 		auto table = reservation_manager->getReservationTable(channel);				
		// 		for (auto slot : slots) {
		// 			table->mark(slot, Reservation(some_other_id, Reservation::BUSY));									
		// 			num_blocked++;
		// 			if (slot > latest_slot)
		// 				latest_slot = slot;
		// 		}
		// 	}						
		// 	CPPUNIT_ASSERT_EQUAL((size_t) (pp_initiator->proposal_num_frequency_channels * pp_initiator->proposal_num_time_slots), num_blocked);
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 150;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until request has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_reply_link_established && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();	
		// 		latest_slot--;
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_GREATER(0, latest_slot);
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_reply_link_established, third_party_link.status);			
		// 	// make sure that initial burst is not touched
		// 	// but later ones are scheduled					
		// 	size_t num_tx_at_initiator = 0, num_tx_at_recipient = 0, num_busy_at_thirdparty = 0, num_scheduled_for_other_link = 0;			
		// 	for (auto *channel : reservation_manager->getP2PFreqChannels()) {				
		// 		const auto *table = reservation_manager->getReservationTable(channel);
		// 		for (size_t t = 0; t < env->planning_horizon; t++) {
		// 			const auto *tbl_initiator = mac_initiator->reservation_manager->getReservationTable(channel);
		// 			const auto *tbl_recipient = mac_recipient->reservation_manager->getReservationTable(channel);
		// 			const auto *tbl_thirdparty = mac->reservation_manager->getReservationTable(channel);
		// 			const Reservation &res_initiator = tbl_initiator->getReservation(t),
		// 						&res_recipient = tbl_recipient->getReservation(t),
		// 						&res_thirdparty = tbl_thirdparty->getReservation(t);						
		// 			if (t < latest_slot) {									
		// 				CPPUNIT_ASSERT(res_thirdparty.isIdle() || res_thirdparty == Reservation(some_other_id, Reservation::BUSY));
		// 				if (res_thirdparty == Reservation(some_other_id, Reservation::BUSY))
		// 					num_scheduled_for_other_link++;
		// 			} else {						
		// 				if (res_initiator.isTx()) {					
		// 					num_tx_at_initiator++;
		// 					CPPUNIT_ASSERT_EQUAL(Reservation(id_initiator, Reservation::RX), res_recipient);
		// 					CPPUNIT_ASSERT_EQUAL(true, res_thirdparty.isBusy());					
		// 				}
		// 				if (res_recipient.isTx()) {
		// 					num_tx_at_recipient++;
		// 					CPPUNIT_ASSERT_EQUAL(Reservation(id_recipient, Reservation::RX), res_initiator);
		// 					CPPUNIT_ASSERT_EQUAL(true, res_thirdparty.isBusy());					
		// 				}
		// 				if (res_thirdparty.isBusy())
		// 					num_busy_at_thirdparty++;							
		// 			}
		// 		}					
		// 	}
		// 	// CPPUNIT_ASSERT_GREATER(size_t(0), num_scheduled_for_other_link);
		// 	// CPPUNIT_ASSERT_EQUAL(num_tx_at_initiator + num_tx_at_recipient, num_busy_at_thirdparty);			
		// }

		// /** Tests that when another third party link terminates, an existing link that is awaiting a reply locks those resources that lie in the present or future. Tests an entire link duration. */
		// void testAnotherLinkResetLocksFutureResources() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until request has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_request_awaiting_reply && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();					
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);			
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_request_awaiting_reply, third_party_link.status);			
		// 	// locks have been made
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link.locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link.locked_resources_for_recipient.size());
		// 	// receive another request that would have locked the same resources
		// 	auto *request = env_initator->phy_layer->outgoing_packets.at(env_initator->phy_layer->outgoing_packets.size() - 1)->copy();
		// 	CPPUNIT_ASSERT(request != nullptr);
		// 	CPPUNIT_ASSERT_GREATER(-1, request->getRequestIndex());
		// 	MacId id_initiator_2 = MacId(id.getId() + 100), id_recipient_2 = MacId(id.getId() + 101);
		// 	((L2HeaderBase*) request->getHeaders().at(0))->src_id = id_initiator_2;
		// 	((L2HeaderLinkRequest*) request->getHeaders().at(request->getRequestIndex()))->dest_id = id_recipient_2;			
		// 	mac->receiveFromLower(request, env->sh_frequency);
		// 	mac->onSlotEnd();
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), mac->getThirdPartyLink(id_initiator_2, id_recipient_2).locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), mac->getThirdPartyLink(id_initiator_2, id_recipient_2).locked_resources_for_recipient.size());
		// 	// now terminate the first link which *has* locked resources
		// 	third_party_link.reset();
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link.locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link.locked_resources_for_recipient.size());
		// 	// notify the other third party link
		// 	mac->onThirdPartyLinkReset(&third_party_link);						
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), mac->getThirdPartyLink(id_initiator_2, id_recipient_2).locked_resources_for_initiator.size());
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), mac->getThirdPartyLink(id_initiator_2, id_recipient_2).locked_resources_for_recipient.size());
		// }

		// /** Tests that when another third party link terminates, an existing link that has received a reply schedules those resources that lie in the present or future. Tests an entire link duration. */
		// void testAnotherLinkResetSchedulesFutureResources() {
		// 	// initiate link establishment
		// 	mac_initiator->notifyOutgoing(1, id_recipient);			
		// 	size_t num_slots = 0, max_slots = 30;
		// 	auto &third_party_link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	// proceed until request has been received
		// 	while (third_party_link.status != ThirdPartyLink::Status::received_reply_link_established && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();					
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);			
		// 	CPPUNIT_ASSERT_EQUAL(ThirdPartyLink::received_reply_link_established, third_party_link.status);			
		// 	// reservations have been made
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), third_party_link.scheduled_resources.size());			
		// 	// receive another reply that would have scheduled the same resources
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), env_recipient->phy_layer->outgoing_packets.size());
		// 	auto *reply = env_recipient->phy_layer->outgoing_packets.at(env_recipient->phy_layer->outgoing_packets.size() - 1)->copy();
		// 	CPPUNIT_ASSERT(reply != nullptr);
		// 	CPPUNIT_ASSERT_GREATER(-1, reply->getReplyIndex());
		// 	MacId id_initiator_2 = MacId(id.getId() + 100), id_recipient_2 = MacId(id.getId() + 101);
		// 	((L2HeaderBase*) reply->getHeaders().at(0))->src_id = id_initiator_2;
		// 	((L2HeaderLinkRequest*) reply->getHeaders().at(reply->getReplyIndex()))->dest_id = id_recipient_2;			
		// 	mac->receiveFromLower(reply, env->sh_frequency);
		// 	mac->onSlotEnd();
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), mac->getThirdPartyLink(id_initiator_2, id_recipient_2).scheduled_resources.size());			
		// 	// now terminate the first link which *has* scheduled resources
		// 	third_party_link.reset();
		// 	CPPUNIT_ASSERT_EQUAL(size_t(0), third_party_link.scheduled_resources.size());			
		// 	// notify the other third party link
		// 	mac->onThirdPartyLinkReset(&third_party_link);						
		// 	CPPUNIT_ASSERT_GREATER(size_t(0), mac->getThirdPartyLink(id_initiator_2, id_recipient_2).scheduled_resources.size());			
		// }
		
		// /** Encountered bug where the link reply would contain a different no. of slots than the request. This was ignored by the link initiator, but not by third parties and caused a crash due to a bad_alloc (burst_length_rx was consequently negative). */
		// void testLinkParametersDontChange() {			
		// 	pp_initiator->notifyOutgoing(1);
		// 	size_t num_slots = 0, max_slots = 100;
		// 	size_t num_req_tx_slots_you = 2;
		// 	while (mac_recipient->stat_num_requests_rcvd.get() < 1 && num_slots++ < max_slots) {
		// 		pp_recipient->outgoing_traffic_estimate.put(env_recipient->phy_layer->getCurrentDatarate() * num_req_tx_slots_you);
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();														
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_recipient->stat_num_requests_rcvd.get());			
		// 	CPPUNIT_ASSERT_EQUAL(num_req_tx_slots_you, (size_t) pp_recipient->getRequiredTxSlots());
		// 	num_slots = 0;
		// 	while (mac->stat_num_third_party_replies_rcvd.get() < 1 && num_slots++ < max_slots) {
		// 		pp_recipient->outgoing_traffic_estimate.put(env_recipient->phy_layer->getCurrentDatarate() * num_req_tx_slots_you);
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_initiator->stat_num_replies_rcvd.get());
		// 	CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_third_party_replies_rcvd.get());
		// 	num_slots = 0;
		// 	while ((pp_initiator->link_status != LinkManager::link_established || pp_recipient->link_status != LinkManager::link_established) && num_slots++ < max_slots) {
		// 		mac_initiator->update(1);
		// 		mac_recipient->update(1);
		// 		mac->update(1);
		// 		mac_initiator->execute();
		// 		mac_recipient->execute();
		// 		mac->execute();
		// 		mac_initiator->onSlotEnd();
		// 		mac_recipient->onSlotEnd();
		// 		mac->onSlotEnd();
		// 	}
		// 	CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_initiator->link_status);
		// 	CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_recipient->link_status);			
		// 	const ThirdPartyLink &link = mac->getThirdPartyLink(id_initiator, id_recipient);
		// 	CPPUNIT_ASSERT_EQUAL(uint(1), pp_initiator->link_state.burst_length_tx);
		// 	CPPUNIT_ASSERT_EQUAL(uint(1), pp_recipient->link_state.burst_length_tx);	
		// 	CPPUNIT_ASSERT_EQUAL(1, link.link_description.burst_length_tx);
		// 	CPPUNIT_ASSERT_EQUAL(uint(2), pp_initiator->link_state.burst_length);
		// 	CPPUNIT_ASSERT_EQUAL(uint(2), pp_recipient->link_state.burst_length);
		// 	CPPUNIT_ASSERT_EQUAL(2, link.link_description.burst_length);
		// 	CPPUNIT_ASSERT_EQUAL(uint(1), pp_initiator->link_state.burst_length_rx);
		// 	CPPUNIT_ASSERT_EQUAL(uint(1), pp_recipient->link_state.burst_length_rx);
		// 	CPPUNIT_ASSERT_EQUAL(1, link.link_description.burst_length_rx);
		// }


		CPPUNIT_TEST_SUITE(ThirdPartyLinkTests);		
			CPPUNIT_TEST(testGetThirdPartyLink);			
			CPPUNIT_TEST(testLinkRequestLocks);
			// CPPUNIT_TEST(testMissingReplyUnlocks);
			// CPPUNIT_TEST(testExpectedReply);			
			// CPPUNIT_TEST(testUnscheduleAfterTimeHasPassed);			
			// CPPUNIT_TEST(testNoLocksAfterLinkExpiry);
			// CPPUNIT_TEST(testResourceAgreementsMatchOverDurationOfOneLink);
			// CPPUNIT_TEST(testLinkReestablishment);
			// CPPUNIT_TEST(testTwoLinkRequestsWithSameResources);			
			// CPPUNIT_TEST(testImmediateResetUnlocks);
			// CPPUNIT_TEST(testResetJustBeforeReplyUnlocks);			
			// CPPUNIT_TEST(testImmediateResetUnschedules);
			// CPPUNIT_TEST(testIntermediateResetUnschedules);
			// CPPUNIT_TEST(testLateResetUnschedules);						
			// CPPUNIT_TEST(testRequestLocksWherePossible);
			// CPPUNIT_TEST(testRequestSchedulesExpectedReply);	
			// CPPUNIT_TEST(testLinkRequestOverwritesBroadcast);
			// CPPUNIT_TEST(testLinkRequestOverwritesBeacon);		
			// CPPUNIT_TEST(testReplyUnlocks);
			// CPPUNIT_TEST(testUnexpectedReply);
			// CPPUNIT_TEST(testRequestAndReplySaveLinkInfo);
			// CPPUNIT_TEST(testReplySchedulesBursts);
			// CPPUNIT_TEST(testReplySchedulesBurstsButDoesNotOverwrite);
			// CPPUNIT_TEST(testAnotherLinkResetLocksFutureResources);
			// CPPUNIT_TEST(testAnotherLinkResetSchedulesFutureResources);
			// CPPUNIT_TEST(testLinkParametersDontChange);
		CPPUNIT_TEST_SUITE_END();
	};

}