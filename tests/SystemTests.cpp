//
// Created by Sebastian Lindner on 10.12.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"
#include "../BCLinkManager.hpp"
#include "../LinkManagementEntity.hpp"
#include "../BCLinkManagementEntity.hpp"


namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * These tests aim at both sides of a communication link, so that e.g. link renewal can be properly tested,
	 * ensuring that both sides are in valid states at all times.
	 */
	class SystemTests : public CppUnit::TestFixture {
	private:
		TestEnvironment* env_me, * env_you;

		MacId own_id, communication_partner_id;
		uint32_t planning_horizon;
		uint64_t center_frequency1, center_frequency2, center_frequency3, bc_frequency, bandwidth;
		NetworkLayer* net_layer_me, * net_layer_you;
		RLCLayer* rlc_layer_me, * rlc_layer_you;
		ARQLayer* arq_layer_me, * arq_layer_you;
		MACLayer* mac_layer_me, * mac_layer_you;
		PHYLayer* phy_layer_me, * phy_layer_you;

	public:
		void setUp() override {
			own_id = MacId(42);
			communication_partner_id = MacId(43);
			env_me = new TestEnvironment(own_id, communication_partner_id);
			env_you = new TestEnvironment(communication_partner_id, own_id);

			center_frequency1 = env_me->center_frequency1;
			center_frequency2 = env_me->center_frequency2;
			center_frequency3 = env_me->center_frequency3;
			bc_frequency = env_me->bc_frequency;
			bandwidth = env_me->bandwidth;
			planning_horizon = env_me->planning_horizon;

			net_layer_me = env_me->net_layer;
			net_layer_you = env_you->net_layer;
			rlc_layer_me = env_me->rlc_layer;
			rlc_layer_you = env_you->rlc_layer;
			arq_layer_me = env_me->arq_layer;
			arq_layer_you = env_you->arq_layer;
			mac_layer_me = env_me->mac_layer;
			mac_layer_you = env_you->mac_layer;
			phy_layer_me = env_me->phy_layer;
			phy_layer_you = env_you->phy_layer;

			phy_layer_me->connected_phy = phy_layer_you;
			phy_layer_you->connected_phy = phy_layer_me;
		}

		void tearDown() override {
			delete env_me;
			delete env_you;
		}

		/**
		 * Schedules a single broadcast message and updates time until it has been received.
		 */
		void testBroadcast() {
//				coutd.setVerbose(true);
			// Single message.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			CPPUNIT_ASSERT_EQUAL(size_t(0), rlc_layer_you->receptions.size());
			// Notify about outgoing data, which schedules a broadcast slot.
			mac_layer_me->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
			// While it is scheduled, increment time.
			size_t num_slots = 0, max_slots = 10;
			while (((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->broadcast_slot_scheduled && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			// Ensure that it has been received.
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
//				coutd.setVerbose(false);
		}

		/**
		 * Notifies one communication partner of an outgoing message for the other partner.
		 * This sends a request, which the partner replies to, until the link is established.
		 * It is also ensured that corresponding future slot reservations are marked.
		 */
		void testLinkEstablishment() {
//			coutd.setVerbose(true);
			// Single message.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			size_t num_slots = 0, max_slots = 20;
			LinkManager* lm_me = mac_layer_me->getLinkManager(communication_partner_id);
			LinkManager* lm_you = mac_layer_you->getLinkManager(own_id);
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->statistic_num_received_packets);
			while (((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->broadcast_slot_scheduled && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);

			// Link request should've been sent, so we're 'awaiting_reply', and they're not established (as the reply hasn't been sent yet).
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_me->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, lm_you->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_you->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_you->statistic_num_received_requests);
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->statistic_num_sent_packets);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->lme->default_tx_timeout, lm_me->lme->tx_timeout);
			// Increment time until status is 'link_established'.
			num_slots = 0;
			while (mac_layer_me->getLinkManager(communication_partner_id)->link_establishment_status != LinkManager::link_established && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			// Link reply should've arrived, so *our* link should be established...
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_me->getLinkManager(communication_partner_id)->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_me->statistic_num_received_packets);
			// ... and *their* link should indicate that the reply has been sent.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_data_tx, mac_layer_you->getLinkManager(own_id)->link_establishment_status);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->lme->default_tx_timeout, lm_me->lme->tx_timeout);
			// Make sure that all corresponding slots are marked as TX on our side,
			ReservationTable* table_me = lm_me->current_reservation_table;
			ReservationTable* table_you = lm_you->current_reservation_table;
			for (size_t offset = lm_me->lme->tx_offset; offset < lm_me->lme->tx_timeout * lm_me->lme->tx_offset; offset += lm_me->lme->tx_offset) {
				const Reservation& reservation_tx = table_me->getReservation(offset);
				const Reservation& reservation_rx = table_you->getReservation(offset);
				coutd << "t=" << offset << " " << reservation_tx << ":" << *table_me->getLinkedChannel() << " " << reservation_rx << ":" << *table_you->getLinkedChannel() << std::endl;
				CPPUNIT_ASSERT_EQUAL(true, reservation_tx.isTx());
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation_tx.getTarget());
				// and one RX where the first data transmission is expected is marked on their side.
				if (offset == lm_me->lme->tx_offset)
					CPPUNIT_ASSERT(reservation_rx == Reservation(own_id, Reservation::RX));
				else
					CPPUNIT_ASSERT_EQUAL(true, reservation_rx.isIdle());
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_you->statistic_num_received_packets);
			// Jump in time to the next transmission.
			mac_layer_me->update(lm_you->lme->tx_offset);
			mac_layer_you->update(lm_you->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			// *Their* status should now show an established link.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_you->getLinkManager(own_id)->link_establishment_status);
			// Reservation timeout should be 1 less now.
			CPPUNIT_ASSERT_EQUAL(lm_me->lme->default_tx_timeout - 1, lm_me->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(2), rlc_layer_you->receptions.size());
			CPPUNIT_ASSERT_EQUAL(size_t(2), lm_you->statistic_num_received_packets);
			// Ensure reservations now match: one side has TX, other side has RX.
			for (size_t offset = lm_me->lme->tx_offset; offset < lm_me->lme->tx_timeout * lm_me->lme->tx_offset; offset += lm_me->lme->tx_offset) {
				const Reservation& reservation_tx = table_me->getReservation(offset);
				const Reservation& reservation_rx = table_you->getReservation(offset);
				coutd << "t=" << offset << " " << reservation_tx << ":" << *table_me->getLinkedChannel() << " " << reservation_rx << ":" << *table_you->getLinkedChannel() << std::endl;
				CPPUNIT_ASSERT_EQUAL(true, reservation_tx.isTx());
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation_tx.getTarget());
				CPPUNIT_ASSERT_EQUAL(true, reservation_rx.isRx());
				CPPUNIT_ASSERT_EQUAL(own_id, reservation_rx.getTarget());
			}
//			coutd.setVerbose(false);
		}


		/**
		 * Notifies one communication partner of an outgoing message for the other partner.
		 * This sends a request, which the partner replies to, until the link is established.
		 * It differs from testLinkEstablishment as the traffic estimation suggests to use multi-slot transmission bursts.
		 * It is also ensured that corresponding future slot reservations are marked.
		 */
		void testLinkEstablishmentMultiSlotBurst() {
//            coutd.setVerbose(true);
			// Single message.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			LinkManager* lm_me = mac_layer_me->getLinkManager(communication_partner_id);
			LinkManager* lm_you = mac_layer_you->getLinkManager(own_id);
			// Update traffic estimate s.t. multi-slot bursts should be used.
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			lm_me->updateTrafficEstimate(expected_num_slots * bits_per_slot);
			unsigned int required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(expected_num_slots * bits_per_slot, communication_partner_id);
			while (((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->broadcast_slot_scheduled) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				lm_me->updateTrafficEstimate(expected_num_slots * bits_per_slot);
				required_slots = lm_me->estimateCurrentNumSlots();
				CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			}
			// Ensure that the request requested a multi-slot reservation.
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer_me->outgoing_packets.size());
			L2Packet* request = phy_layer_me->outgoing_packets.at(0);
			CPPUNIT_ASSERT(request->getRequestIndex() > -1);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, request->getHeaders().at(request->getRequestIndex())->frame_type);
			required_slots = lm_me->estimateCurrentNumSlots();
			auto* request_payload = (LinkManagementEntity::ProposalPayload*) request->getPayloads().at(request->getRequestIndex());
			CPPUNIT_ASSERT_EQUAL(required_slots, request_payload->burst_length);
			// Link request should've been sent, so we're 'awaiting_reply'.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_me->link_establishment_status);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->lme->default_tx_timeout, lm_me->lme->tx_timeout);
			// Increment time until status is 'link_established'.
			while (mac_layer_me->getLinkManager(communication_partner_id)->link_establishment_status != LinkManager::link_established) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Link reply should've arrived, so *our* link should be established...
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_me->getLinkManager(communication_partner_id)->link_establishment_status);
			// ... and *their* link should indicate that the reply has been sent.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_data_tx, mac_layer_you->getLinkManager(own_id)->link_establishment_status);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->lme->default_tx_timeout, lm_me->lme->tx_timeout);

			// Make sure that all corresponding slots are marked as TX on our side,
			ReservationTable* table_me = lm_me->current_reservation_table;
			ReservationTable* table_you = lm_you->current_reservation_table;
			for (size_t offset = lm_me->lme->tx_offset; offset < lm_me->lme->tx_timeout * lm_me->lme->tx_offset; offset += lm_me->lme->tx_offset) {
				const Reservation& reservation_tx = table_me->getReservation(offset);
				const Reservation& reservation_rx = table_you->getReservation(offset);
				coutd << "t=" << offset << " " << reservation_tx << ":" << *table_me->getLinkedChannel() << " " << reservation_rx << ":" << *table_you->getLinkedChannel() << std::endl;
				CPPUNIT_ASSERT_EQUAL(true, reservation_tx.isTx());
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation_tx.getTarget());
				// and one RX where the first data transmission is expected is marked on their side.
				if (offset == lm_me->lme->tx_offset)
					CPPUNIT_ASSERT(reservation_rx == Reservation(own_id, Reservation::RX));
				else
					CPPUNIT_ASSERT_EQUAL(true, reservation_rx.isIdle());
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_you->statistic_num_received_packets);
			// Wait until the next transmission.
			mac_layer_me->update(lm_you->lme->tx_offset);
			mac_layer_you->update(lm_you->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			// *Their* status should now show an established link.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_you->getLinkManager(own_id)->link_establishment_status);
			// Reservation timeout should be 1 less now.
			CPPUNIT_ASSERT_EQUAL(lm_me->lme->default_tx_timeout - 1, lm_me->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(2), rlc_layer_you->receptions.size());
			// Ensure reservations match now, with multi-slot TX and matching multi-slot RX.
			for (size_t offset = lm_me->lme->tx_offset; offset < lm_me->lme->tx_timeout * lm_me->lme->tx_offset; offset += lm_me->lme->tx_offset) {
				const Reservation& reservation_tx = table_me->getReservation(offset);
				const Reservation& reservation_rx = table_you->getReservation(offset);
				coutd << "t=" << offset << " " << reservation_tx << ":" << *table_me->getLinkedChannel() << " " << reservation_rx << ":" << *table_you->getLinkedChannel() << std::endl;
				unsigned int remaining_slots = (unsigned int) expected_num_slots - 1;
				CPPUNIT_ASSERT_EQUAL(true, reservation_tx.isTx());
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation_tx.getTarget());
				CPPUNIT_ASSERT_EQUAL(remaining_slots, reservation_tx.getNumRemainingSlots());
				CPPUNIT_ASSERT_EQUAL(true, reservation_rx.isRx());
				CPPUNIT_ASSERT_EQUAL(own_id, reservation_rx.getTarget());
				CPPUNIT_ASSERT_EQUAL(remaining_slots, reservation_rx.getNumRemainingSlots());
				for (size_t t = 1; t <= remaining_slots; t++) {
					const Reservation &reservation_tx_next = table_me->getReservation(offset + t);
					const Reservation &reservation_rx_next = table_you->getReservation(offset + t);
					CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX_CONT, reservation_tx_next.getAction());
					CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation_tx_next.getTarget());
					CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, reservation_rx_next.getAction());
					CPPUNIT_ASSERT_EQUAL(own_id, reservation_rx_next.getTarget());
				}
			}
//            coutd.setVerbose(false);
		}

		/**
		 * Tests that a link expires when the timeout is reached.
		 */
		void testLinkExpiry() {
			// Establish link and send first burst.
			testLinkEstablishment();
			// Don't try to renew the link.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			LinkManager* lm_me = mac_layer_me->getLinkManager(communication_partner_id);
			LinkManager* lm_you = mac_layer_you->getLinkManager(own_id);
			unsigned int expected_tx_timeout = lm_me->lme->default_tx_timeout - 1;
			CPPUNIT_ASSERT_EQUAL(expected_tx_timeout, lm_me->lme->tx_timeout);
			// Now increment time until the link expires.
			size_t num_slots = 0, max_num_slots = 100;
			while (lm_me->link_establishment_status != LinkManager::link_not_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_me->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_you->link_establishment_status);
			for (const auto& channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
				const ReservationTable *table_me = mac_layer_me->getReservationManager()->getReservationTable(channel),
										*table_you = mac_layer_you->getReservationManager()->getReservationTable(channel);
				for (size_t t = 1; t < planning_horizon; t++) {
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table_me->getReservation(t));
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table_you->getReservation(t));
				}
			}
		}

		void testLinkExpiryMultiSlot() {
			LinkManager* lm_me = mac_layer_me->getLinkManager(communication_partner_id);
			LinkManager* lm_you = mac_layer_you->getLinkManager(own_id);
			// Update traffic estimate s.t. multi-slot bursts should be used.
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			lm_me->updateTrafficEstimate(expected_num_slots * bits_per_slot);
			unsigned int required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// Now do the other tests.
			testLinkExpiry();
		}

		/**
		 * Tests link renewal with a channel change.
		 */
		void testLinkRenewalChannelChange() {
//			coutd.setVerbose(true);
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			LinkManager* lm_tx = mac_layer_me->getLinkManager(communication_partner_id),
						*lm_rx = mac_layer_you->getLinkManager(own_id);
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			while (lm_rx->link_establishment_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Link establishment should've worked.
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			// One data transmission should've happened (which established the link at RX).
			CPPUNIT_ASSERT_EQUAL(lm_tx->lme->default_tx_timeout - 1, lm_tx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(lm_rx->lme->default_tx_timeout - 1, lm_rx->lme->tx_timeout);

			// Proceed until the first request.
			CPPUNIT_ASSERT(lm_tx->lme->max_num_renewal_attempts > 0);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			num_slots = 0;
			while (lm_tx->lme->scheduled_requests.size() > lm_tx->lme->max_num_renewal_attempts - 1 && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(true, lm_tx->lme->link_renewal_pending);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(true, lm_rx->lme->link_renewal_pending);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);

			// Proceed until the reply has been received.
			num_slots = 0, max_num_slots = 25;
			while ((lm_tx->link_establishment_status != LinkManager::link_renewal_complete) && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_renewal_complete, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(false, lm_tx->lme->link_renewal_pending);
			// RX doesn't know whether its reply has been received yet.
			CPPUNIT_ASSERT_EQUAL(true, lm_rx->lme->link_renewal_pending);
			// Channel change (comes from proposing two channels by default, out of three, and the most idle ones are used <-> won't reuse the current channel).
			CPPUNIT_ASSERT(*lm_tx->lme->next_channel != *lm_tx->current_channel);
			// They should agree on the new channel.
			CPPUNIT_ASSERT(*lm_tx->lme->next_channel == *lm_rx->lme->next_channel);

			// Proceed until the link is renewed.
			num_slots = 0;
			while ((lm_tx->link_establishment_status != LinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(*lm_tx->current_channel, *lm_rx->current_channel);
			CPPUNIT_ASSERT( lm_tx->lme->next_channel == nullptr);
			CPPUNIT_ASSERT( lm_rx->lme->next_channel == nullptr);
			CPPUNIT_ASSERT_EQUAL(lm_tx->lme->default_tx_timeout, lm_tx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(lm_rx->lme->default_tx_timeout, lm_rx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_rx->lme->scheduled_requests.size());

			// Proceed until first reservation of new link.
			do {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			} while (lm_tx->current_reservation_table->getReservation(0).isIdle());

			// Make sure that all requests target correct slots.
			for (size_t t = 0; t <= lm_tx->lme->getExpiryOffset(); t++) {
				// Make sure there's no reservations on any other channel for both TX and RX.
				size_t num_res_other_channels_tx = 0, num_res_other_channels_rx = 0;
				for (const auto* channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
					if (*channel != *lm_tx->current_channel) {
						const Reservation& res_tx = mac_layer_me->reservation_manager->getReservationTable(channel)->getReservation(t);
						if (!res_tx.isIdle())
							num_res_other_channels_tx++;
						const Reservation& res_rx = mac_layer_you->reservation_manager->getReservationTable(channel)->getReservation(t);
						if (!res_rx.isIdle())
							num_res_other_channels_rx++;
					}
				}
				CPPUNIT_ASSERT_EQUAL(size_t(0), num_res_other_channels_tx);
				CPPUNIT_ASSERT_EQUAL(size_t(0), num_res_other_channels_rx);

				const Reservation& reservation_tx = lm_tx->current_reservation_table->getReservation(t);
				const Reservation& reservation_rx = lm_rx->current_reservation_table->getReservation(t);
				coutd << "t=" << t << ": " << reservation_tx << "|" << reservation_rx;
				size_t current_slot = mac_layer_me->getCurrentSlot();
				if (std::any_of(lm_tx->lme->scheduled_requests.begin(), lm_tx->lme->scheduled_requests.end(), [t, current_slot](uint64_t t_request){
					return t_request - current_slot == t;
				})) {
					coutd << " REQUEST";
					// Request slots should match TX slots.
					CPPUNIT_ASSERT_EQUAL(Reservation(communication_partner_id, Reservation::TX), reservation_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), lm_rx->current_reservation_table->getReservation(t));
				}
				coutd << std::endl;
				// Matching reservations.
				if (t % lm_tx->lme->tx_offset == 0) {
					CPPUNIT_ASSERT_EQUAL(Reservation(communication_partner_id, Reservation::TX), reservation_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), lm_rx->current_reservation_table->getReservation(t));
				}
			}

//			coutd.setVerbose(true);

			// Proceed until the next renewal has been negotiated.
			num_slots = 0, max_num_slots = 100;
			while (lm_tx->lme->scheduled_requests.size() > lm_tx->lme->max_num_renewal_attempts - 1 && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(Reservation(communication_partner_id, Reservation::RX), lm_tx->current_reservation_table->getReservation(lm_tx->lme->tx_offset));
			CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::TX), lm_rx->current_reservation_table->getReservation(lm_tx->lme->tx_offset));
			// Proceed until the reply has been sent.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_renewal_complete, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete, lm_rx->link_establishment_status);
			// Proceed until the link is renewed.
			num_slots = 0;
			while ((lm_tx->link_establishment_status != LinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(*lm_tx->current_channel, *lm_rx->current_channel);
			CPPUNIT_ASSERT_EQUAL(lm_tx->lme->default_tx_timeout, lm_tx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(lm_rx->lme->default_tx_timeout, lm_rx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_rx->lme->scheduled_requests.size());

//			coutd.setVerbose(false);
		}

		void testLinkRenewalSameChannel() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Blacklist two out of three P2P channels.
			mac_layer_me->reservation_manager->getFreqChannelByCenterFreq(center_frequency2)->setBlacklisted(true);
			mac_layer_me->reservation_manager->getFreqChannelByCenterFreq(center_frequency3)->setBlacklisted(true);
			mac_layer_you->reservation_manager->getFreqChannelByCenterFreq(center_frequency2)->setBlacklisted(true);
			mac_layer_you->reservation_manager->getFreqChannelByCenterFreq(center_frequency3)->setBlacklisted(true);
			// Restrict number of proposed channels to one.
			LinkManager* lm_tx = mac_layer_me->getLinkManager(communication_partner_id),
					*lm_rx = mac_layer_you->getLinkManager(own_id);
			lm_tx->lme->num_proposed_channels = 1;
			lm_rx->lme->num_proposed_channels = 1;

			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			while (lm_rx->link_establishment_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Link establishment should've worked.
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			// One data transmission should've happened (which established the link at RX).
			CPPUNIT_ASSERT_EQUAL(lm_tx->lme->default_tx_timeout - 1, lm_tx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(lm_rx->lme->default_tx_timeout - 1, lm_rx->lme->tx_timeout);

			// Proceed until the first request.
			CPPUNIT_ASSERT(lm_tx->lme->max_num_renewal_attempts > 0);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			num_slots = 0;
			while (lm_tx->lme->scheduled_requests.size() > lm_tx->lme->max_num_renewal_attempts - 1 && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(true, lm_tx->lme->link_renewal_pending);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(true, lm_rx->lme->link_renewal_pending);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);

			// Proceed until the reply has been received.
			num_slots = 0, max_num_slots = 25;
			while ((lm_tx->link_establishment_status != LinkManager::link_renewal_complete) && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_renewal_complete, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(false, lm_tx->lme->link_renewal_pending);
			// RX doesn't know whether its reply has been received yet.
			CPPUNIT_ASSERT_EQUAL(true, lm_rx->lme->link_renewal_pending);
			// No channel change.
			CPPUNIT_ASSERT(*lm_tx->lme->next_channel == *lm_tx->current_channel);
			// They should agree on the new channel.
			CPPUNIT_ASSERT(*lm_tx->lme->next_channel == *lm_rx->lme->next_channel);

//			coutd.setVerbose(true);

			// Proceed until the link is renewed.
			num_slots = 0;
			while ((lm_tx->link_establishment_status != LinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(*lm_tx->current_channel, *lm_rx->current_channel);
			CPPUNIT_ASSERT( lm_tx->lme->next_channel == nullptr);
			CPPUNIT_ASSERT( lm_rx->lme->next_channel == nullptr);
			CPPUNIT_ASSERT_EQUAL(lm_tx->lme->default_tx_timeout, lm_tx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(lm_rx->lme->default_tx_timeout, lm_rx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_rx->lme->scheduled_requests.size());

			// Proceed until first reservation of new link.
			do {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			} while (lm_tx->current_reservation_table->getReservation(0).isIdle());

			// Make sure that all requests target correct slots.
			for (size_t t = 0; t <= lm_tx->lme->getExpiryOffset(); t++) {
				// Make sure there's no reservations on any other channel for both TX and RX.
				size_t num_res_other_channels_tx = 0, num_res_other_channels_rx = 0;
				for (const auto* channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
					if (*channel != *lm_tx->current_channel) {
						const Reservation& res_tx = mac_layer_me->reservation_manager->getReservationTable(channel)->getReservation(t);
						if (!res_tx.isIdle())
							num_res_other_channels_tx++;
						const Reservation& res_rx = mac_layer_you->reservation_manager->getReservationTable(channel)->getReservation(t);
						if (!res_rx.isIdle())
							num_res_other_channels_rx++;
					}
				}
				CPPUNIT_ASSERT_EQUAL(size_t(0), num_res_other_channels_tx);
				CPPUNIT_ASSERT_EQUAL(size_t(0), num_res_other_channels_rx);

				const Reservation& reservation_tx = lm_tx->current_reservation_table->getReservation(t);
				const Reservation& reservation_rx = lm_rx->current_reservation_table->getReservation(t);
				coutd << "t=" << t << ": " << reservation_tx << "|" << reservation_rx;
				size_t current_slot = mac_layer_me->getCurrentSlot();
				if (std::any_of(lm_tx->lme->scheduled_requests.begin(), lm_tx->lme->scheduled_requests.end(), [t, current_slot](uint64_t t_request){
					return t_request - current_slot == t;
				})) {
					coutd << " REQUEST";
					// Request slots should match TX slots.
					CPPUNIT_ASSERT_EQUAL(Reservation(communication_partner_id, Reservation::TX), reservation_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), lm_rx->current_reservation_table->getReservation(t));
				}
				coutd << std::endl;
				// Matching reservations.
				if (t % lm_tx->lme->tx_offset == 0) {
					CPPUNIT_ASSERT_EQUAL(Reservation(communication_partner_id, Reservation::TX), reservation_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), lm_rx->current_reservation_table->getReservation(t));
				}
			}

//			coutd.setVerbose(true);

			// Proceed until the next renewal has been negotiated.
			num_slots = 0, max_num_slots = 100;
			while (lm_tx->lme->scheduled_requests.size() > lm_tx->lme->max_num_renewal_attempts - 1 && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(Reservation(communication_partner_id, Reservation::RX), lm_tx->current_reservation_table->getReservation(lm_tx->lme->tx_offset));
			CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::TX), lm_rx->current_reservation_table->getReservation(lm_tx->lme->tx_offset));
			// Proceed until the reply has been sent.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_renewal_complete, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete, lm_rx->link_establishment_status);
			// Proceed until the link is renewed.
			num_slots = 0;
			while ((lm_tx->link_establishment_status != LinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(*lm_tx->current_channel, *lm_rx->current_channel);
			CPPUNIT_ASSERT_EQUAL(lm_tx->lme->default_tx_timeout, lm_tx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(lm_rx->lme->default_tx_timeout, lm_rx->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_rx->lme->scheduled_requests.size());
//			coutd.setVerbose(false);
		}

		/**
		 * Link timeout threshold is reached.
		 * Ensures that a 3rd reply to a 3rd request are is sent if the first two replies are lost.
		 */
		void testLinkExpiringAndLostRequest() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			LinkManager* lm_tx = mac_layer_me->getLinkManager(communication_partner_id),
					*lm_rx = mac_layer_you->getLinkManager(own_id);
			// Do three renewal attempts.
			lm_tx->lme->max_num_renewal_attempts = 3;
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			while (lm_rx->link_establishment_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			auto next_request = *std::min_element(lm_tx->lme->scheduled_requests.begin(), lm_tx->lme->scheduled_requests.end());
			// Proceed to the burst *before* the request is sent.
//			coutd.setVerbose(true);
			CPPUNIT_ASSERT(next_request > mac_layer_me->getCurrentSlot() + lm_tx->lme->tx_offset);
			while (next_request > mac_layer_me->getCurrentSlot() + lm_tx->lme->tx_offset) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Make sure it hasn't been sent yet.
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			// *Drop* the next packet.
			phy_layer_me->connected_phy = nullptr;
			phy_layer_you->connected_phy = nullptr;
			// Proceed to the request slot.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts - 1), lm_tx->lme->scheduled_requests.size());
			// Proceed to the next request.
			next_request = *std::min_element(lm_tx->lme->scheduled_requests.begin(), lm_tx->lme->scheduled_requests.end());
			while (next_request > mac_layer_me->getCurrentSlot()) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_tx->lme->scheduled_requests.size());
			// Reconnect.
			phy_layer_me->connected_phy = phy_layer_you;
			phy_layer_you->connected_phy = phy_layer_me;
			// Last request should be received.
			next_request = *std::min_element(lm_tx->lme->scheduled_requests.begin(), lm_tx->lme->scheduled_requests.end());
			while (next_request > mac_layer_me->getCurrentSlot()) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// ALl requests are sent.
			CPPUNIT_ASSERT_EQUAL(true, lm_tx->lme->scheduled_requests.empty());
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_tx->link_establishment_status);
			// Proceed until reply is sent.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete, lm_rx->link_establishment_status);
			// This was the last reserved slot, so the next one will apply the link renewal.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			CPPUNIT_ASSERT_EQUAL(*lm_tx->current_channel, *lm_rx->current_channel);
			CPPUNIT_ASSERT_EQUAL(false, lm_tx->lme->link_renewal_pending);
			CPPUNIT_ASSERT_EQUAL(false, lm_rx->lme->link_renewal_pending);
		}

		/**
		 * Link timeout threshold is reached.
		 * Ensures that a 2nd request is sent if the first one's reply was dropped, and that reservations made for this first request are cleared once the second one is taken.
		 */
		void testLinkExpiringAndLostReply() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			LinkManager* lm_tx = mac_layer_me->getLinkManager(communication_partner_id),
					*lm_rx = mac_layer_you->getLinkManager(own_id);
			// Do three renewal attempts.
			lm_tx->lme->max_num_renewal_attempts = 3;
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			while (lm_rx->link_establishment_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			auto next_request = *std::min_element(lm_tx->lme->scheduled_requests.begin(), lm_tx->lme->scheduled_requests.end());
			// Proceed to the burst *before* the request is sent.
//			coutd.setVerbose(true);
			CPPUNIT_ASSERT(next_request > mac_layer_me->getCurrentSlot() + lm_tx->lme->tx_offset);
			while (next_request > mac_layer_me->getCurrentSlot() + lm_tx->lme->tx_offset) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Make sure it hasn't been sent yet.
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			// *Drop* the next reply.
			phy_layer_you->connected_phy = nullptr;
			// Proceed to the request slot.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts - 1), lm_tx->lme->scheduled_requests.size());
			// Proceed to the next request.
			next_request = *std::min_element(lm_tx->lme->scheduled_requests.begin(), lm_tx->lme->scheduled_requests.end());
			while (next_request > mac_layer_me->getCurrentSlot()) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Proceed to reply, which is still lost.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_tx->lme->scheduled_requests.size());
			// Reconnect -> last reply should be received.
			phy_layer_you->connected_phy = phy_layer_me;
			next_request = *std::min_element(lm_tx->lme->scheduled_requests.begin(), lm_tx->lme->scheduled_requests.end());
			while (next_request > mac_layer_me->getCurrentSlot()) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}

			// ALl requests are sent.
			CPPUNIT_ASSERT_EQUAL(true, lm_tx->lme->scheduled_requests.empty());
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_tx->link_establishment_status);
			// Proceed until reply is sent.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete, lm_rx->link_establishment_status);
			// This was the last reserved slot, so the next one will apply the link renewal.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_tx->lme->max_num_renewal_attempts), lm_tx->lme->scheduled_requests.size());
			CPPUNIT_ASSERT_EQUAL(*lm_tx->current_channel, *lm_rx->current_channel);
			CPPUNIT_ASSERT_EQUAL(false, lm_tx->lme->link_renewal_pending);
			CPPUNIT_ASSERT_EQUAL(false, lm_rx->lme->link_renewal_pending);

			while (lm_tx->current_reservation_table->getReservation(0).isIdle()) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}

			// Reservations made at TX and RX from earlier requests should've been cleared.
			size_t num_res_tx = 0, num_res_rx = 0, num_res_additional_tx = 0, num_res_additional_rx = 0;
			for (size_t t = 0; t < lm_tx->lme->getExpiryOffset() * 2; t++) {
				for (const FrequencyChannel* channel : lm_tx->reservation_manager->getP2PFreqChannels()) {
					const Reservation& res_tx = lm_tx->reservation_manager->getReservationTable(channel)->getReservation(t);
					const Reservation& res_rx = lm_rx->reservation_manager->getReservationTable(channel)->getReservation(t);
					coutd << "t=" << t << " f=" << *channel << ": " << res_tx << " | " << res_rx << std::endl;
					if (*channel == *lm_tx->current_channel) {
						if (!res_tx.isIdle()) {
							num_res_tx++;
							CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), res_rx);
							CPPUNIT_ASSERT(t % lm_tx->lme->tx_offset == 0);
						}
						if (!res_rx.isIdle()) {
							num_res_rx++;
							CPPUNIT_ASSERT_EQUAL(Reservation(communication_partner_id, Reservation::TX), res_tx);
							CPPUNIT_ASSERT(t % lm_tx->lme->tx_offset == 0);
						}
					} else {
						if (!res_tx.isIdle())
							num_res_additional_tx++;
						if (!res_rx.isIdle())
							num_res_additional_rx++;
					}
				}
			}
			size_t expected_num_res = lm_tx->lme->default_tx_timeout;
			CPPUNIT_ASSERT_EQUAL(expected_num_res, num_res_tx);
			CPPUNIT_ASSERT_EQUAL(expected_num_res, num_res_rx);
			// No reservations on other channels should exist.
			CPPUNIT_ASSERT_EQUAL(size_t(0), num_res_additional_tx);
			CPPUNIT_ASSERT_EQUAL(size_t(0), num_res_additional_rx);
		}

		/**
		 * TODO
		 * Link timeout threshold is reached.
		 * Ensures that if no negotiation has happened prior to expiry, the link is reset to unestablished.
		 */
		void testLinkRenewalFails() {
//			coutd.setVerbose(true);
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			LinkManager* lm_tx = mac_layer_me->getLinkManager(communication_partner_id),
					*lm_rx = mac_layer_you->getLinkManager(own_id);
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			while (lm_rx->link_establishment_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);

			// From now on, drop all packets.
			phy_layer_me->connected_phy = nullptr;
			phy_layer_you->connected_phy = nullptr;

			num_slots = 0;
			while (lm_tx->link_establishment_status != LinkManager::link_not_established) {
				mac_layer_me->update(lm_tx->lme->tx_offset);
				mac_layer_you->update(lm_tx->lme->tx_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT(lm_tx->current_channel == nullptr);
			CPPUNIT_ASSERT(lm_tx->current_reservation_table == nullptr);
			CPPUNIT_ASSERT(lm_rx->current_channel == nullptr);
			CPPUNIT_ASSERT(lm_rx->current_reservation_table == nullptr);

			for (auto channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
				for (size_t t = 1; t < planning_horizon; t++) {
					const Reservation& res_tx = mac_layer_me->getReservationManager()->getReservationTable(channel)->getReservation(t);
					const Reservation& res_rx = mac_layer_you->getReservationManager()->getReservationTable(channel)->getReservation(t);
					if (!res_tx.isIdle() || !res_rx.isIdle())
						coutd << "f=" << *channel << " t=" << t << ": " << res_tx << "|" << res_rx << std::endl;
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res_rx);
				}
			}
		}

		void testLinkRenewalAfterExpiry() {
			testLinkRenewalFails();
			// Reconnect.
			phy_layer_me->connected_phy = phy_layer_you;
			phy_layer_you->connected_phy = phy_layer_me;
			// Now try link establishment again.
//			coutd.setVerbose(true);
			size_t num_slots = 0, max_num_slots = 100;
			LinkManager* lm_tx = mac_layer_me->getLinkManager(communication_partner_id),
					*lm_rx = mac_layer_you->getLinkManager(own_id);
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			while (lm_rx->link_establishment_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_tx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_rx->link_establishment_status);
		}

		/**
		 * Observed a crash in OMNeT++: after link renewal was negotiated, the sender was informed of more data to send, which somehow crashed the system.
		 */
		void testSimulatorScenario() {
//			coutd.setVerbose(true);
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Proceed up to a negotiated link renewal.
			size_t num_slots = 0, max_num_slots = 100;
			LinkManager* lm_tx = mac_layer_me->getLinkManager(communication_partner_id),
					*lm_rx = mac_layer_you->getLinkManager(own_id);
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			while (lm_tx->link_establishment_status != LinkManager::Status::link_renewal_complete && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Proceed to one more data transmission.
			mac_layer_me->update(lm_tx->lme->tx_offset);
			mac_layer_you->update(lm_tx->lme->tx_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			// Proceed one more slot.
			mac_layer_me->update(1);
			mac_layer_you->update(1);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			mac_layer_me->notifyOutgoing(1024, communication_partner_id);
		}

		/** Before introducing the onSlotEnd() function, success depended on the order of the execute() calls (which is of course terrible),
		 * so this test ensures that the order in which user actions are executed doesn't matter.
		 */
		void testCommunicateInOtherDirection() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			LinkManager* lm_tx = mac_layer_me->getLinkManager(communication_partner_id),
					*lm_rx = mac_layer_you->getLinkManager(own_id);
			// Other guy tries to communicate with us.
			mac_layer_you->notifyOutgoing(512, own_id);
			while (lm_tx->link_establishment_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_tx->link_establishment_status);
		}

		/** Before introducing the onSlotEnd() function, success depended on the order of the execute() calls (which is of course terrible),
		 * so this test ensures that the order in which user actions are executed doesn't matter.
		 */
		void testCommunicateReverseOrder() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			LinkManager* lm_tx = mac_layer_me->getLinkManager(communication_partner_id),
					*lm_rx = mac_layer_you->getLinkManager(own_id);
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			while (lm_rx->link_establishment_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				// you first, then me
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_tx->link_establishment_status);
		}

		// TODO
		void testEncapsulatedUnicast() {
		}

		// TODO
		void testRequestDoesntMakeIt() {
		}

		// TODO
		void testReplyDoesntMakeIt() {

		}

	CPPUNIT_TEST_SUITE(SystemTests);
//			CPPUNIT_TEST(testBroadcast);
//			CPPUNIT_TEST(testLinkEstablishment);
//            CPPUNIT_TEST(testLinkEstablishmentMultiSlotBurst);
            CPPUNIT_TEST(testLinkExpiry);
			CPPUNIT_TEST(testLinkExpiryMultiSlot);
//			CPPUNIT_TEST(testLinkRenewalChannelChange);
//			CPPUNIT_TEST(testLinkRenewalSameChannel);
//			CPPUNIT_TEST(testLinkExpiringAndLostRequest);
//			CPPUNIT_TEST(testLinkExpiringAndLostReply);
//			CPPUNIT_TEST(testLinkRenewalFails);
//			CPPUNIT_TEST(testLinkRenewalAfterExpiry);
//			CPPUNIT_TEST(testSimulatorScenario);
//			CPPUNIT_TEST(testCommunicateInOtherDirection);
//			CPPUNIT_TEST(testCommunicateReverseOrder);
//			CPPUNIT_TEST(testEncapsulatedUnicast);
		CPPUNIT_TEST_SUITE_END();
	};

}