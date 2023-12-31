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
		CPPUNIT_ASSERT_EQUAL(partner_id, sh->link_requests.at(0).first);
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
		mac->update(1);
		mac_you->update(1);
		mac->execute();
		mac_you->execute();
		mac->onSlotEnd();
		mac_you->onSlotEnd();
		CPPUNIT_ASSERT_EQUAL(true, sh->next_broadcast_scheduled);		
		CPPUNIT_ASSERT_GREATER(uint(0), sh->next_broadcast_slot);

		// construct link request
		L2Packet *packet = mac_you->requestSegment(100, SYMBOLIC_LINK_ID_BROADCAST);
		L2HeaderSH *&header = (L2HeaderSH*&) packet->getHeaders().at(0);
		CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::broadcast, header->frame_type);
		header->src_id = partner_id;
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

		// ensure own link establishment has been triggered
		CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_request_generation, pp->link_status);
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
		L2Packet *packet = mac_you->requestSegment(100, SYMBOLIC_LINK_ID_BROADCAST);
		L2HeaderSH *&header = (L2HeaderSH*&) packet->getHeaders().at(0);
		CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::broadcast, header->frame_type);
		header->src_id = partner_id;
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
		L2Packet *packet = mac_you->requestSegment(100, SYMBOLIC_LINK_ID_BROADCAST);
		L2HeaderSH *&header = (L2HeaderSH*&) packet->getHeaders().at(0);
		CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::broadcast, header->frame_type);
		LinkProposal proposal = LinkProposal();
		proposal.period = 3;
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

	void testProposalLinkEstablishment() {
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

		num_slots = 0;
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

	/** Tests that the first packet of a newly-established PP link is sent. */
	void testUnicastPacketIsSent() {
		size_t num_slots = 0, max_slots = 250;
		mac->notifyOutgoing(1, partner_id);
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
		num_slots = 0;
		while (mac->stat_num_unicasts_sent.get() < 1.0 && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_unicasts_sent.get());		
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_unicasts_rcvd.get());		
	}

	void testNextTxSlotCorrectlySetAfterLinkEstablishment() {
		size_t num_slots = 0, max_slots = 250;
		mac->notifyOutgoing(1, partner_id);
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
		CPPUNIT_ASSERT(pp->current_reservation_table != nullptr);
		CPPUNIT_ASSERT(pp_you->current_reservation_table != nullptr);				
		CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, pp->current_reservation_table->getReservation(pp->getNextTxSlot()).getAction());		
		CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, pp->current_reservation_table->getReservation(pp_you->getNextTxSlot()).getAction());
		CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, pp_you->current_reservation_table->getReservation(pp_you->getNextTxSlot()).getAction());
		CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, pp_you->current_reservation_table->getReservation(pp->getNextTxSlot()).getAction());
	}

	void testIsStartOfTxBurst() {
		size_t num_slots = 0, max_slots = 250;
		mac->notifyOutgoing(1, partner_id);
		env->rlc_layer->should_there_be_more_p2p_data = false;  // don't reestablish the link		
		env_you->rlc_layer->should_there_be_more_p2p_data = false;  // don't reestablish the link		
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
		max_slots = 1000;
		num_slots = 0;
		size_t num_tx_slots = 0;
		while (pp->link_status != LinkManager::link_not_established && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			if (std::any_of(mac->reservation_manager->getP2PReservationTables().begin(), mac->reservation_manager->getP2PReservationTables().end(), [](ReservationTable *tbl) { return tbl->getReservation(0).isTx(); })) {
				num_tx_slots++;
				CPPUNIT_ASSERT_EQUAL(true, pp->isStartOfTxBurst());
			}
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_EQUAL(size_t(mac->getDefaultPPLinkTimeout()), num_tx_slots);
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp->link_status);
	}

	void testReportStartAndEndOfTxBurstsToArq() {
		size_t num_slots = 0, max_slots = 250;
		mac->notifyOutgoing(1, partner_id);
		env->rlc_layer->should_there_be_more_p2p_data = false;  // don't reestablish the link		
		env_you->rlc_layer->should_there_be_more_p2p_data = false;  // don't reestablish the link		
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
		max_slots = 1000;
		num_slots = 0;
		size_t num_tx_slots = 0;
		bool expecting_report;
		while (pp->link_status != LinkManager::link_not_established && num_slots++ < max_slots) {
			expecting_report = false;
			mac->update(1);
			mac_you->update(1);
			if (std::any_of(mac->reservation_manager->getP2PReservationTables().begin(), mac->reservation_manager->getP2PReservationTables().end(), [](ReservationTable *tbl) { return tbl->getReservation(0).isTx(); })) {
				num_tx_slots++;
				expecting_report = true;				
			}
			CPPUNIT_ASSERT_EQUAL(false, pp->reported_start_tx_burst_to_arq);
			CPPUNIT_ASSERT_EQUAL(false, pp->reported_end_tx_burst_to_arq);
			mac->execute();
			mac_you->execute();
			if (expecting_report) {
				CPPUNIT_ASSERT_EQUAL(true, pp->reported_start_tx_burst_to_arq);
				CPPUNIT_ASSERT_EQUAL(false, pp->reported_end_tx_burst_to_arq);
			}
			mac->onSlotEnd();
			mac_you->onSlotEnd();
			if (expecting_report) {
				CPPUNIT_ASSERT_EQUAL(true, pp->reported_start_tx_burst_to_arq);
				CPPUNIT_ASSERT_EQUAL(true, pp->reported_end_tx_burst_to_arq);
			}
		}
		CPPUNIT_ASSERT_EQUAL(size_t(mac->getDefaultPPLinkTimeout()), num_tx_slots);
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp->link_status);
	}

	void testReportMissingPacketToArq() {
		size_t num_slots = 0, max_slots = 250;
		mac->notifyOutgoing(1, partner_id);
		env->rlc_layer->should_there_be_more_p2p_data = false;  // don't reestablish the link		
		env_you->rlc_layer->should_there_be_more_p2p_data = false;  // don't reestablish the link		
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
		max_slots = 1000;
		num_slots = 0;
		bool expect_missing_packet = false;
		// drop all packets going A->B
		env->phy_layer->connected_phys.clear();
		while (!expect_missing_packet && num_slots++ < max_slots) {			
			mac->update(1);
			mac_you->update(1);
			if (std::any_of(mac->reservation_manager->getP2PReservationTables().begin(), mac->reservation_manager->getP2PReservationTables().end(), [](ReservationTable *tbl) { return tbl->getReservation(0).isTx(); })) {
				expect_missing_packet = true;
			}			
			mac->execute();
			mac_you->execute();			
			mac->onSlotEnd();
			mac_you->onSlotEnd();
			if (expect_missing_packet) 
				CPPUNIT_ASSERT_EQUAL(true, pp_you->reported_missing_packet_to_arq);							
		}		
		CPPUNIT_ASSERT_EQUAL(true, expect_missing_packet);
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);				
	}

	/** Tests that throughout an entire PP link, the timeouts between two users match and are correctly decremented. */
	void testTimeoutsMatchOverWholePPLink() {
		env->rlc_layer->should_there_be_more_p2p_data = false;
		env_you->rlc_layer->should_there_be_more_p2p_data = false;
		size_t num_slots = 0, max_slots = 250;		
		while ((pp->link_status != LinkManager::link_established || pp_you->link_status != LinkManager::link_established) && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
			if (num_slots == 20)
				mac->notifyOutgoing(1, partner_id);
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp->link_status);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_you->link_status);		
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_pp_links_established.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_pp_links_established.get());

		// intially, timeouts should be max on both sides
		CPPUNIT_ASSERT_EQUAL(mac->getDefaultPPLinkTimeout(), pp->getRemainingTimeout());
		CPPUNIT_ASSERT_EQUAL(mac->getDefaultPPLinkTimeout(), pp_you->getRemainingTimeout());		
		for (int timeout = 0; timeout < mac->getDefaultPPLinkTimeout(); timeout++) {
			int next_burst_end = pp->is_link_initiator ? pp->getNextRxSlot() : pp_you->getNextRxSlot();			
			for (int t = 0; t < next_burst_end; t++) {
				mac->update(1);
				mac_you->update(1);
				mac->execute();
				mac_you->execute();
				mac->onSlotEnd();
				mac_you->onSlotEnd();
			}			
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_pp_links_established.get());
			CPPUNIT_ASSERT_EQUAL(size_t(timeout+1), (size_t) mac->stat_num_unicasts_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(timeout+1), (size_t) mac_you->stat_num_unicasts_sent.get());
			CPPUNIT_ASSERT_EQUAL(mac->getDefaultPPLinkTimeout() - (timeout+1), pp->getRemainingTimeout());
			CPPUNIT_ASSERT_EQUAL(mac->getDefaultPPLinkTimeout() - (timeout+1), pp_you->getRemainingTimeout());		
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_EQUAL(0, pp->getRemainingTimeout());
		CPPUNIT_ASSERT_EQUAL(0, pp_you->getRemainingTimeout());
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp->link_status);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp_you->link_status);		
	}	

	/** Tests that when two users attempt to establish links to one another, the first received link request cancels the other's attempt. */
	void testCancelLinkRequestWhenRequestIsReceived() {
		mac->notifyOutgoing(1, partner_id);
		mac_you->notifyOutgoing(1, id);
		CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
		CPPUNIT_ASSERT_EQUAL(size_t(1), sh_you->link_requests.size());
		size_t num_slots = 0, max_slots = 100;
		while ((pp->link_status != LinkManager::link_established && pp_you->link_status != LinkManager::link_established) && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();			
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT(size_t(mac->stat_num_requests_rcvd.get() + mac_you->stat_num_requests_rcvd.get()) >= size_t(1) && size_t(mac->stat_num_requests_rcvd.get() + mac_you->stat_num_requests_rcvd.get()) <= size_t(sh->num_proposals_unadvertised_link_requests));
	}

	void testLinkReestablishmentWhenTheresMoreData() {
		env->rlc_layer->should_there_be_more_p2p_data = true;
		env_you->rlc_layer->should_there_be_more_p2p_data = true;
		mac->notifyOutgoing(1, partner_id);
		size_t num_slots = 0, max_slots = 3000;		
		while ((mac->stat_num_pp_links_established.get() < 2.0) && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();			
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);		
		CPPUNIT_ASSERT(LinkManager::link_not_established != pp->link_status);		
		CPPUNIT_ASSERT(LinkManager::link_not_established != pp_you->link_status);				
		CPPUNIT_ASSERT_EQUAL(size_t(2), (size_t) mac->stat_num_pp_links_established.get());		
	}


	/** Tests that the next_tx_in variable is always accurate. */
	void testNextTxSlotCorrectlySetOverWholePPLink() {
		size_t num_slots = 0, max_slots = 3000;
		mac->notifyOutgoing(1, partner_id);
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

		num_slots = 0;
		while (mac->stat_num_pp_links_expired.get() < 1.0 && num_slots++ < max_slots) {
			try {								
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), pp->current_reservation_table->getReservation(pp->getNextTxSlot()));
			} catch (const std::runtime_error &e) {
				// do nothing
			}			
			try {								
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), pp->current_reservation_table->getReservation(pp->getNextRxSlot()));
			} catch (const std::runtime_error &e) {
				// do nothing
			}			
			try {								
				CPPUNIT_ASSERT_EQUAL(Reservation(id, Reservation::TX), pp_you->current_reservation_table->getReservation(pp_you->getNextTxSlot()));
			} catch (const std::runtime_error &e) {
				// do nothing
			}			
			try {								
				CPPUNIT_ASSERT_EQUAL(Reservation(id, Reservation::RX), pp_you->current_reservation_table->getReservation(pp_you->getNextRxSlot()));
			} catch (const std::runtime_error &e) {
				// do nothing
			}			
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_pp_links_expired.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_pp_links_expired.get());
	}

	/** Tests that after link establishment, an entire PP link communication works and packets are exchanged. */
	void testCommOverWholePPLink() {
		env->rlc_layer->should_there_be_more_p2p_data = false;
		env_you->rlc_layer->should_there_be_more_p2p_data = false;
		size_t num_slots = 0, max_slots = 250;		
		while ((pp->link_status != LinkManager::link_established || pp_you->link_status != LinkManager::link_established) && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
			if (num_slots == 20)
				mac->notifyOutgoing(1, partner_id);
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp->link_status);		
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, pp_you->link_status);		
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_pp_links_established.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_pp_links_established.get());

		// no packets have been exchanged yet
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_unicasts_sent.get());
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_you->stat_num_unicasts_sent.get());
		// proceed until link expiry
		num_slots = 0; 
		max_slots = 3000;		
		while ((mac->stat_num_pp_links_expired.get() < 1.0) && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();			
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_pp_links_expired.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_pp_links_expired.get());
		CPPUNIT_ASSERT_EQUAL(size_t(mac->getDefaultPPLinkTimeout()), (size_t) mac->stat_num_unicasts_sent.get());
		CPPUNIT_ASSERT_EQUAL(size_t(mac->getDefaultPPLinkTimeout()), (size_t) mac_you->stat_num_unicasts_sent.get());		
	}

	void testMaxLinkEstablishmentAttemptsReached() {
		env->rlc_layer->should_there_be_more_p2p_data = false;
		env_you->rlc_layer->should_there_be_more_p2p_data = false;
		mac->notifyOutgoing(1, partner_id);
		// disconnect
		env->phy_layer->connected_phys.clear();
		size_t num_slots = 0, max_slots = 500;
		while (mac->stat_num_requests_sent.get() < 1.0 && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		// request sent
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_requests_sent.get());
		// but not received
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_requests_rcvd.get());
		// expected reply slot should be set
		int expected_reply_slot = pp->expected_link_request_confirmation_slot;
		CPPUNIT_ASSERT_GREATER(0, expected_reply_slot);
		for (int t = 0; t < expected_reply_slot + 1; t++) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_pp_link_missed_last_reply_opportunity.get());
		CPPUNIT_ASSERT_EQUAL(size_t(2), (size_t) pp->establishment_attempts);
		// now continue until max attempts have been reached
		num_slots = 0;
		while (mac->stat_pp_link_exceeded_max_no_establishment_attempts.get() < 1.0 && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_pp_link_exceeded_max_no_establishment_attempts.get());
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_pp_links_established.get());
		CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, pp->link_status);
	}	

	void testPPLinkEstablishmentTime() {
		int num_time_slots_before_start = 10;
		for (int t = 0; t < num_time_slots_before_start; t++) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		mac->notifyOutgoing(1, partner_id);
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_you->stat_num_pp_links_established.get());
		size_t num_slots = 0, max_slots = 512;
		while (pp->link_status != LinkManager::Status::link_established && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, pp->link_status);
		CPPUNIT_ASSERT_GREATER(0.0, mac_you->stat_pp_link_establishment_time.get());				
		CPPUNIT_ASSERT_EQUAL(mac_you->stat_pp_link_establishment_time.get(), mac->stat_pp_link_establishment_time.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_pp_links_established.get());			
	}

	void testManyPPLinkEstablishmentTimes() {
		env->rlc_layer->should_there_be_more_p2p_data = true;
		env_you->rlc_layer->should_there_be_more_p2p_data = true;
		mac_you->notifyOutgoing(1, id);
		size_t num_slots = 0, max_slots = 3000, num_links = 2;
		std::vector<double> link_establishment_times, link_establishment_times_you;
		while ((mac->stat_num_pp_links_established.get() < num_links) && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();			
			if (mac->stat_num_pp_links_established.get() > link_establishment_times.size())
				link_establishment_times.push_back(mac->stat_pp_link_establishment_time.get());			
			if (mac_you->stat_num_pp_links_established.get() > link_establishment_times_you.size())
				link_establishment_times_you.push_back(mac_you->stat_pp_link_establishment_time.get());							
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);				
		CPPUNIT_ASSERT_EQUAL(num_links, (size_t) mac->stat_num_pp_links_established.get());
		CPPUNIT_ASSERT_EQUAL(num_links, link_establishment_times.size());
		CPPUNIT_ASSERT(link_establishment_times.size() >= link_establishment_times.size() -1 && link_establishment_times.size() <= link_establishment_times.size() + 1);			
		for (size_t i = 0; i < num_links; i++) {
			CPPUNIT_ASSERT_GREATEREQUAL(1.0, link_establishment_times.at(i));			
			if (link_establishment_times_you.size() > i)
				CPPUNIT_ASSERT_EQUAL(link_establishment_times.at(i), link_establishment_times_you.at(i));
		}			
	}

	/** In many simulations, the first data is not transmitted at simulation start, but later. Make sure that this works as expected. */
	void testManyPPLinkEstablishmentTimesStartLate() {
		size_t num_slots_before_start = 1000;
		for (int t = 0; t < num_slots_before_start; t++) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();								
		}
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_you->stat_num_pp_links_established.get());
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac->stat_num_pp_links_established.get());		
		mac_you->notifyOutgoing(1, id);			
		env_you->rlc_layer->should_there_be_more_p2p_data = false;
		CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_you->stat_num_pp_links_established.get());
		size_t num_slots = 0, max_slots = 10000, num_links = 5;
		std::vector<double> link_establishment_times, link_establishment_times_you;
		while (mac_you->stat_num_pp_links_established.get() < num_links && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();								
			if (mac_you->stat_num_pp_links_established.get() > link_establishment_times.size())
				link_establishment_times.push_back(mac_you->stat_pp_link_establishment_time.get());
			if (mac_you->stat_num_pp_links_established.get() > link_establishment_times_you.size())
				link_establishment_times_you.push_back(mac_you->stat_pp_link_establishment_time.get());							
		}		
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_EQUAL(num_links, (size_t) mac_you->stat_num_pp_links_established.get());
		CPPUNIT_ASSERT_EQUAL(num_links, link_establishment_times.size());
		CPPUNIT_ASSERT(link_establishment_times.size() >= link_establishment_times.size() -1 && link_establishment_times.size() <= link_establishment_times.size() + 1);			
		for (size_t i = 0; i < num_links; i++) {
			CPPUNIT_ASSERT_GREATEREQUAL(1.0, link_establishment_times.at(i));
			if (link_establishment_times_you.size() > i)
				CPPUNIT_ASSERT_GREATEREQUAL(1.0, link_establishment_times_you.at(i));				
		}			
	}

	CPPUNIT_TEST_SUITE(PPLinkManagerTests);
		CPPUNIT_TEST(testGet);		
		CPPUNIT_TEST(testAskSHToSendLinkRequest);
		CPPUNIT_TEST(testSendLinkRequestWithNoAdvertisedLink);				
		CPPUNIT_TEST(testSendLinkRequestWithAdvertisedLink);
		CPPUNIT_TEST(testAcceptAdvertisedLinkRequest);
		CPPUNIT_TEST(testStartOwnLinkIfRequestInacceptable);
		CPPUNIT_TEST(testLinkUtilizationIsCorrectAfterEstablishment);
		CPPUNIT_TEST(testResourcesScheduledAfterLinkRequest);				
		CPPUNIT_TEST(testUnlockAfterLinkRequest);		
		CPPUNIT_TEST(testLinkRequestLaterThanNextSHTransmissionIsRejected);						
		CPPUNIT_TEST(testLinkReplySlotOffsetIsNormalized);						
		CPPUNIT_TEST(testProcessLinkReply);		
		CPPUNIT_TEST(testLocalLinkEstablishment);
		CPPUNIT_TEST(testProposalLinkEstablishment);		
		CPPUNIT_TEST(testUnicastPacketIsSent);		
		CPPUNIT_TEST(testCommOverWholePPLink);		
		CPPUNIT_TEST(testNextTxSlotCorrectlySetAfterLinkEstablishment);		
		CPPUNIT_TEST(testIsStartOfTxBurst);				
		CPPUNIT_TEST(testReportStartAndEndOfTxBurstsToArq);						
		CPPUNIT_TEST(testReportMissingPacketToArq);								
		CPPUNIT_TEST(testTimeoutsMatchOverWholePPLink);		
		CPPUNIT_TEST(testCancelLinkRequestWhenRequestIsReceived);		
		CPPUNIT_TEST(testLinkReestablishmentWhenTheresMoreData);		
		CPPUNIT_TEST(testNextTxSlotCorrectlySetOverWholePPLink);		
		CPPUNIT_TEST(testCommOverWholePPLink);
		CPPUNIT_TEST(testMaxLinkEstablishmentAttemptsReached);
		CPPUNIT_TEST(testPPLinkEstablishmentTime);		
		CPPUNIT_TEST(testManyPPLinkEstablishmentTimes);
		CPPUNIT_TEST(testManyPPLinkEstablishmentTimesStartLate);				
		
	CPPUNIT_TEST_SUITE_END();
};
}