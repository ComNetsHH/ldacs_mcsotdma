#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../PPLinkManager.hpp"
#include "../SHLinkManager.hpp"
#include "MockLayers.hpp"
#include "../SlotCalculator.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
class PPLinkManagerTests : public CppUnit::TestFixture {
private:
	PPLinkManager *pp, *pp_you;
	SHLinkManager *sh, *sh_you;
	MacId id, partner_id;
	uint32_t planning_horizon;
	MACLayer *mac, *mac_you;
	TestEnvironment *env, *env_you;

public:
	void setUp() override {
		id = MacId(42);
		partner_id = MacId(43);
		env = new TestEnvironment(id, partner_id);
		env_you = new TestEnvironment(partner_id, id);
		planning_horizon = env->planning_horizon;
		mac = env->mac_layer;		
		mac_you = env_you->mac_layer;
		pp = (PPLinkManager*) mac->getLinkManager(partner_id);
		sh = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
		pp_you = (PPLinkManager*) mac_you->getLinkManager(id);
		sh_you = (SHLinkManager*) mac_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);

		env->phy_layer->connected_phys.push_back(env_you->phy_layer);
		env_you->phy_layer->connected_phys.push_back(env->phy_layer);		
	}

	void tearDown() override {
		delete env;
	}

	void testGet() {
		CPPUNIT_ASSERT(pp != nullptr);
	}

	void testAskSHToSendLinkRequest() {		
		CPPUNIT_ASSERT_EQUAL(false, sh->isNextBroadcastScheduled());
		CPPUNIT_ASSERT_EQUAL(true, sh->link_requests.empty());
		pp->notifyOutgoing(100);
		CPPUNIT_ASSERT_EQUAL(true, sh->isNextBroadcastScheduled());
		CPPUNIT_ASSERT_EQUAL(false, sh->link_requests.empty());
		CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
		CPPUNIT_ASSERT_EQUAL(partner_id, sh->link_requests.at(0));
	}

	/** Tests that when there's no saved, advertised link, the SH initiates a two-way handshake. */
	void testSendLinkRequestWithNoAdvertisedLink() {
		mac->notifyOutgoing(1, partner_id);
		CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
		size_t request_tx_slot = sh->next_broadcast_slot;
		for (size_t t = 0; t < request_tx_slot; t++) {
			mac->update(1);
			mac->execute();
			mac->onSlotEnd();
		}
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_requests_sent.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_own_proposals_sent.get());		
	}

	/** Tests that when there is an advertised link, the SH initiates a 1SHOT establishment. */
	void testSendLinkRequestWithAdvertisedLink() {
		size_t num_slots = 0, max_slots = 50;
		while (mac->stat_num_broadcasts_rcvd.get() < 1.0 && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_you->stat_num_broadcasts_sent.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_broadcasts_rcvd.get());
		// link proposals have been received
		// start link establishment
		mac->notifyOutgoing(1, partner_id);
		size_t request_tx_slot = sh->next_broadcast_slot;
		for (size_t t = 0; t < request_tx_slot; t++) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_requests_sent.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_saved_proposals_sent.get());		
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_own_proposals_sent.get());		
	}

	/** Tests that link request is accepted if possible. */
	void testAcceptAdvertisedLinkRequest() {
		size_t num_slots = 0, max_slots = 250;
		while (mac->stat_num_broadcasts_rcvd.get() < 1.0 && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_GREATEREQUAL(size_t(1), (size_t) mac_you->stat_num_broadcasts_sent.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_broadcasts_rcvd.get());
		// link proposals have been received
		// start link establishment
		mac->notifyOutgoing(1, partner_id);
		size_t request_tx_slot = sh->next_broadcast_slot;
		for (size_t t = 0; t < request_tx_slot; t++) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_requests_sent.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_saved_proposals_sent.get());		
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_own_proposals_sent.get());		
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_requests_rcvd.get());				
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_you->link_status);
	}

	/** Tests that own link establishment is triggered if a link in unacceptable. */
	void testStartOwnLinkIfRequestInacceptable() {
		bool is_implemented = false;
		CPPUNIT_ASSERT_EQUAL(true, is_implemented);
	}

	/** Tests that after accepting a link request, the link utilization is correctly updated. */
	void testLinkUtilizationIsCorrectAfterEstablishment() {
		size_t num_slots = 0, max_slots = 250;
		mac_you->notifyOutgoing(1, id);
		while (pp->link_status != LinkManager::link_established && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp->link_status);
		auto utilizations = mac->getPPLinkUtilizations();
		CPPUNIT_ASSERT_EQUAL(size_t(1), utilizations.size());
		L2HeaderSH::LinkUtilizationMessage utilization = utilizations.at(0);
		CPPUNIT_ASSERT_EQUAL(pp->next_tx_in, utilization.slot_offset);
		CPPUNIT_ASSERT_EQUAL(pp->slot_duration, utilization.slot_duration);
		CPPUNIT_ASSERT_EQUAL(pp->num_initiator_tx, utilization.num_bursts_forward);
		CPPUNIT_ASSERT_EQUAL(pp->num_recipient_tx, utilization.num_bursts_reverse);
		CPPUNIT_ASSERT_EQUAL(pp->period, utilization.period);
		CPPUNIT_ASSERT(pp->channel != nullptr);
		CPPUNIT_ASSERT_EQUAL(pp->channel->getCenterFrequency(), (uint64_t) utilization.center_frequency);
		CPPUNIT_ASSERT_EQUAL(pp->timeout, utilization.timeout);
	}

	void testResourcesScheduledAfterLinkRequest() {
		size_t num_slots = 0, max_slots = 250;
		mac_you->notifyOutgoing(1, id);
		while (pp->link_status != LinkManager::link_established && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp->link_status);
		auto *channel = pp->channel;
		CPPUNIT_ASSERT(channel != nullptr);
		auto *table = mac->getReservationManager()->getReservationTable(channel);
		CPPUNIT_ASSERT(table != nullptr);
		CPPUNIT_ASSERT_GREATER(size_t(0), pp->reserved_resources.size());
		auto &resources = pp->reserved_resources.scheduled_resources;
		for (auto pair : resources) {
			ReservationTable *tbl = pair.first;
			int slot = pair.second;
			CPPUNIT_ASSERT_EQUAL(tbl, table);
			CPPUNIT_ASSERT_EQUAL(true, table->getReservation(slot).isTx() || table->getReservation(slot).isRx());			
		}		
		for (auto *other_table : mac->getReservationManager()->getP2PReservationTables()) {
			if (other_table != table) {
				for (size_t t = 0; t < table->getPlanningHorizon(); t++)
					CPPUNIT_ASSERT_EQUAL(Reservation(), other_table->getReservation(t));
			}
		}		
	}
	
	void testUnlockAfterLinkRequest() {
		mac->notifyOutgoing(1, partner_id);
		size_t num_slots = 0, max_slots = 250;
		while (mac->stat_num_requests_sent.get() < 1.0 && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);		
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_requests_sent.get());		
		pp->cancelLink();
		for (auto *table : mac->getReservationManager()->getP2PReservationTables()) {
			for (int t = 0; t < table->getPlanningHorizon(); t++) {
				if (!table->getReservation(t).isIdle())
					coutd << "t=" << t << std::endl;
				CPPUNIT_ASSERT_EQUAL(Reservation(), table->getReservation(t));
			}
		}
	}

	/** Tests that if a link request denotes a slot offset earlier than the next possible reply opportunity (the next SH transmission), it is rejected. */
	void testLinkRequestLaterThanNextSHTransmissionIsRejected() {
		mac->update(1);
		mac_you->update(1);
		mac->execute();
		mac_you->execute();
		mac->onSlotEnd();
		mac_you->onSlotEnd();
		CPPUNIT_ASSERT_EQUAL(true, sh->next_broadcast_scheduled);		
		CPPUNIT_ASSERT_GREATER(uint(0), sh->next_broadcast_slot);

		// construct link request
		L2Packet *packet = mac_you->requestSegment(100, partner_id);
		L2HeaderSH *&header = (L2HeaderSH*&) packet->getHeaders().at(0);				
		LinkProposal proposal = LinkProposal();
		proposal.center_frequency = mac_you->getReservationManager()->getP2PFreqChannels().at(0)->getCenterFrequency();
		proposal.slot_offset = sh->next_broadcast_slot - 2;
		L2HeaderSH::LinkRequest request = L2HeaderSH::LinkRequest(id, proposal);
		header->link_requests.push_back(request);

		// receive link request
		mac->update(1);
		mac->receiveFromLower(packet, mac->reservation_manager->getBroadcastFreqChannel()->getCenterFrequency());
		mac->execute();
		mac->onSlotEnd();

		// ensure it's been rejected
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_pp_requests_rejected_due_to_unacceptable_reply_slot.get());
	}

	/** Tests that a link reply's slot offset is normalized. E.g. request at t=5, reply at t=7, then the original slot offset must be decremented by 2. */
	void testLinkReplySlotOffsetIsNormalized() {
		mac->update(1);
		mac_you->update(1);
		mac->execute();
		mac_you->execute();
		mac->onSlotEnd();
		mac_you->onSlotEnd();
		CPPUNIT_ASSERT_EQUAL(true, sh->next_broadcast_scheduled);		
		CPPUNIT_ASSERT_GREATER(uint(0), sh->next_broadcast_slot);

		// construct link request
		L2Packet *packet = mac_you->requestSegment(100, partner_id);
		L2HeaderSH *&header = (L2HeaderSH*&) packet->getHeaders().at(0);				
		LinkProposal proposal = LinkProposal();
		proposal.center_frequency = mac_you->getReservationManager()->getP2PFreqChannels().at(0)->getCenterFrequency();
		proposal.slot_offset = sh->next_broadcast_slot + 1;
		L2HeaderSH::LinkRequest request = L2HeaderSH::LinkRequest(id, proposal);
		header->link_requests.push_back(request);

		// receive link request
		mac->update(1);
		mac->receiveFromLower(packet, mac->reservation_manager->getBroadcastFreqChannel()->getCenterFrequency());
		mac->execute();
		mac->onSlotEnd();

		// ensure it's been accepted
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_pp_link_requests_accepted.get());
		// and a link reply should be pending
		CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_replies.size());
		const L2HeaderSH::LinkReply &reply = sh->link_replies.at(0);
		CPPUNIT_ASSERT_EQUAL(2, reply.proposed_link.slot_offset);
	}

	/** Tests that a link reply is correctly processed. */
	void testProcessLinkReply() {
		size_t num_slots = 0, max_slots = 250;
		mac_you->notifyOutgoing(1, id);
		while (pp->link_status != LinkManager::link_established && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);		
		int slot_offset_until_reply = sh->next_broadcast_slot;
		CPPUNIT_ASSERT_GREATER(0, slot_offset_until_reply);
		for (int t = 0; t < slot_offset_until_reply; t++) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}		
		// expect link reply right now
		CPPUNIT_ASSERT_EQUAL(0, pp->expected_link_request_confirmation_slot);
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_replies_sent.get());		
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_replies_rcvd.get());		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp->link_status);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_you->link_status);
		CPPUNIT_ASSERT_EQUAL(pp->channel->getCenterFrequency(), pp_you->channel->getCenterFrequency());
		auto *table = mac->getReservationManager()->getReservationTable(pp->channel), *table_you = mac_you->getReservationManager()->getReservationTable(pp_you->channel);
		for (int t = 0; t < table->getPlanningHorizon(); t++) {
			const auto &res = table->getReservation(t), &res_you = table_you->getReservation(t);
			if (res.isTx()) {
				if (res_you != Reservation(id, Reservation::RX))
					std::cout << std::endl << "t=" << t << std::endl;
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), res);
				CPPUNIT_ASSERT_EQUAL(Reservation(id, Reservation::RX), res_you);				
			}
			if (res.isRx()) {
				if (res_you != Reservation(id, Reservation::TX))
					std::cout << std::endl << "t=" << t << std::endl;
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), res);
				CPPUNIT_ASSERT_EQUAL(Reservation(id, Reservation::TX), res_you);				
			}
		}
	}

	/** Tests that links are established at both sides when no proposals were present. */
	void testLocalLinkEstablishment() {
		size_t num_slots = 0, max_slots = 250;
		mac_you->notifyOutgoing(1, id);
		while ((pp->link_status != LinkManager::link_established || pp_you->link_status != LinkManager::link_established) && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp->link_status);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_you->link_status);
	}

	CPPUNIT_TEST_SUITE(PPLinkManagerTests);
		CPPUNIT_TEST(testGet);		
		CPPUNIT_TEST(testAskSHToSendLinkRequest);
		CPPUNIT_TEST(testSendLinkRequestWithNoAdvertisedLink);				
		CPPUNIT_TEST(testSendLinkRequestWithAdvertisedLink);
		CPPUNIT_TEST(testAcceptAdvertisedLinkRequest);
		// CPPUNIT_TEST(testStartOwnLinkIfRequestInacceptable);
		CPPUNIT_TEST(testLinkUtilizationIsCorrectAfterEstablishment);
		CPPUNIT_TEST(testResourcesScheduledAfterLinkRequest);				
		CPPUNIT_TEST(testUnlockAfterLinkRequest);		
		CPPUNIT_TEST(testLinkRequestLaterThanNextSHTransmissionIsRejected);				
		CPPUNIT_TEST(testLinkReplySlotOffsetIsNormalized);						
		CPPUNIT_TEST(testProcessLinkReply);		
		CPPUNIT_TEST(testLocalLinkEstablishment);				
	CPPUNIT_TEST_SUITE_END();
};
}