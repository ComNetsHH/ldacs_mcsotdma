//
// Created by seba on 2/24/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"
#include "../LinkManager.hpp"
#include "../P2PLinkManager.hpp"
#include "../BCLinkManager.hpp"


namespace TUHH_INTAIRNET_MCSOTDMA {
	/**
	 * These tests aim at both sides of a communication link, so that e.g. link renewal can be properly tested,
	 * ensuring that both sides are in valid states at all times.
	 */
	class NewSystemTests : public CppUnit::TestFixture {
	private:
		TestEnvironment* env_me, * env_you;

		MacId own_id, partner_id;
		uint32_t planning_horizon;
		uint64_t center_frequency1, center_frequency2, center_frequency3, bc_frequency, bandwidth;
		NetworkLayer* net_layer_me, * net_layer_you;
		RLCLayer* rlc_layer_me, * rlc_layer_you;
		ARQLayer* arq_layer_me, * arq_layer_you;
		MACLayer* mac_layer_me, * mac_layer_you;
		PHYLayer* phy_layer_me, * phy_layer_you;
		size_t num_outgoing_bits;

		P2PLinkManager *lm_me, *lm_you;

	public:
		void setUp() override {
			own_id = MacId(42);
			partner_id = MacId(43);
			env_me = new TestEnvironment(own_id, partner_id, true);
			env_you = new TestEnvironment(partner_id, own_id, true);

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

			num_outgoing_bits = 512;
			lm_me = (P2PLinkManager*) mac_layer_me->getLinkManager(partner_id);
			lm_you = (P2PLinkManager*) mac_layer_you->getLinkManager(own_id);
		}

		void tearDown() override {
			delete env_me;
			delete env_you;
		}

		void testLinkEstablishment() {
//			coutd.setVerbose(true);
			// Single message.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 20;
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->statistic_num_received_packets);
			while (((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->next_broadcast_scheduled && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);

			// Link request should've been sent, so we're 'awaiting_reply', and they're awaiting the first data transmission.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_data_tx, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->statistic_num_received_packets); // forwarded by the BCLinkManager
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_you->statistic_num_received_requests);
			CPPUNIT_ASSERT_EQUAL(size_t(1), ((BCLinkManager*) mac_layer_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(1), ((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->statistic_num_sent_packets);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout, lm_you->current_link_state->timeout);

			// Increment time until status is 'link_established'.
			num_slots = 0;
			while (((LinkManager*) mac_layer_me->getLinkManager(partner_id))->link_status != LinkManager::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			// Link reply should've arrived, so *our* link should be established...
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_me->getLinkManager(partner_id)->link_status);
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_me->statistic_num_received_packets);
			// ... and *their* link should indicate that the reply has been sent.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_data_tx, mac_layer_you->getLinkManager(own_id)->link_status);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout, lm_you->current_link_state->timeout);
			// Make sure that all corresponding slots are marked as TX on our side,
			ReservationTable* table_me = lm_me->current_reservation_table;
			ReservationTable* table_you = lm_you->current_reservation_table;
			for (size_t offset = lm_me->burst_offset; offset < lm_me->current_link_state->timeout * lm_me->burst_offset; offset += lm_me->burst_offset) {
				const Reservation& reservation_tx = table_me->getReservation(offset);
				const Reservation& reservation_rx = table_you->getReservation(offset);
				coutd << "t=" << offset << " " << reservation_tx << ":" << *table_me->getLinkedChannel() << " " << reservation_rx << ":" << *table_you->getLinkedChannel() << std::endl;
				CPPUNIT_ASSERT_EQUAL(true, reservation_tx.isTx());
				CPPUNIT_ASSERT_EQUAL(partner_id, reservation_tx.getTarget());
				// and one RX where the first data transmission is expected is marked on their side.
				if (offset == lm_me->burst_offset)
					CPPUNIT_ASSERT(reservation_rx == Reservation(own_id, Reservation::RX));
				else
					CPPUNIT_ASSERT_EQUAL(true, reservation_rx.isIdle());
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->statistic_num_received_packets); // just the request which had been forwarded by the BCLinkManager
			// Jump in time to the next transmission.
			mac_layer_me->update(lm_you->burst_offset);
			mac_layer_you->update(lm_you->burst_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			// *Their* status should now show an established link.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
//			// Reservation timeout should be 1 less now.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout - 1, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(2), rlc_layer_you->receptions.size());
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_you->statistic_num_received_packets);
			// Ensure reservations now match: one side has TX, other side has RX.
			for (size_t offset = lm_me->burst_offset; offset < lm_me->current_link_state->timeout * lm_me->burst_offset; offset += lm_me->burst_offset) {
				const Reservation& reservation_tx = table_me->getReservation(offset);
				const Reservation& reservation_rx = table_you->getReservation(offset);
				coutd << "t=" << offset << " " << reservation_tx << ":" << *table_me->getLinkedChannel() << " " << reservation_rx << ":" << *table_you->getLinkedChannel() << std::endl;
				CPPUNIT_ASSERT_EQUAL(true, reservation_tx.isTx());
				CPPUNIT_ASSERT_EQUAL(partner_id, reservation_tx.getTarget());
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
			rlc_layer_me->should_there_be_more_p2p_data = true;
			// Update traffic estimate s.t. multi-slot bursts should be used.
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			lm_me->outgoing_traffic_estimate.put(expected_num_slots * bits_per_slot);
			unsigned int required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(expected_num_slots * bits_per_slot, partner_id);
			while (((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->next_broadcast_scheduled) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				lm_me->outgoing_traffic_estimate.put(expected_num_slots * bits_per_slot);
				required_slots = lm_me->estimateCurrentNumSlots();
				CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			}
			// Ensure that the request requested a multi-slot reservation.
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer_me->outgoing_packets.size());
			L2Packet* request = phy_layer_me->outgoing_packets.at(0);
			CPPUNIT_ASSERT(request->getRequestIndex() > -1);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, request->getHeaders().at(request->getRequestIndex())->frame_type);
			required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT(required_slots > 1);
			// Link request should've been sent, so we're 'awaiting_reply'.
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::awaiting_reply, lm_me->link_status);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);
			// Increment time until status is 'link_established'.
			while (mac_layer_me->getLinkManager(partner_id)->link_status != P2PLinkManager::link_established) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Link reply should've arrived, so *our* link should be established...
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, mac_layer_me->getLinkManager(partner_id)->link_status);
			// ... and *their* link should indicate that the reply has been sent.
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::awaiting_data_tx, mac_layer_you->getLinkManager(own_id)->link_status);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);

			// Make sure that all corresponding slots are marked as TX on our side,
			ReservationTable* table_me = lm_me->current_reservation_table;
			ReservationTable* table_you = lm_you->current_reservation_table;
			for (size_t offset = lm_me->burst_offset; offset < lm_me->current_link_state->timeout * lm_me->burst_offset; offset += lm_me->burst_offset) {
				const Reservation& reservation_tx = table_me->getReservation(offset);
				const Reservation& reservation_rx = table_you->getReservation(offset);
				coutd << "t=" << offset << " " << reservation_tx << ":" << *table_me->getLinkedChannel() << " " << reservation_rx << ":" << *table_you->getLinkedChannel() << std::endl;
				CPPUNIT_ASSERT_EQUAL(true, reservation_tx.isTx());
				CPPUNIT_ASSERT_EQUAL(partner_id, reservation_tx.getTarget());
				// and one RX where the first data transmission is expected is marked on their side.
				if (offset == lm_me->burst_offset)
					CPPUNIT_ASSERT(reservation_rx == Reservation(own_id, Reservation::RX));
				else
					CPPUNIT_ASSERT_EQUAL(true, reservation_rx.isIdle());
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->statistic_num_received_packets);
			// Wait until the next transmission.
			mac_layer_me->update(lm_you->burst_offset);
			mac_layer_you->update(lm_you->burst_offset);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			// *Their* status should now show an established link.
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, mac_layer_you->getLinkManager(own_id)->link_status);
			// Reservation timeout should be 1 less now.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(2), rlc_layer_you->receptions.size());
			// Ensure reservations match now, with multi-slot TX and matching multi-slot RX.
			for (size_t offset = lm_me->burst_offset; offset < lm_me->current_link_state->timeout * lm_me->burst_offset; offset += lm_me->burst_offset) {
				const Reservation& reservation_tx = table_me->getReservation(offset);
				const Reservation& reservation_rx = table_you->getReservation(offset);
				coutd << "t=" << offset << " " << reservation_tx << ":" << *table_me->getLinkedChannel() << " " << reservation_rx << ":" << *table_you->getLinkedChannel() << std::endl;
				unsigned int remaining_slots = (unsigned int) expected_num_slots - 1;
				CPPUNIT_ASSERT_EQUAL(true, reservation_tx.isTx());
				CPPUNIT_ASSERT_EQUAL(partner_id, reservation_tx.getTarget());
				CPPUNIT_ASSERT_EQUAL(remaining_slots, reservation_tx.getNumRemainingSlots());
				CPPUNIT_ASSERT_EQUAL(true, reservation_rx.isRx());
				CPPUNIT_ASSERT_EQUAL(own_id, reservation_rx.getTarget());
				CPPUNIT_ASSERT_EQUAL(remaining_slots, reservation_rx.getNumRemainingSlots());
				for (size_t t = 1; t <= remaining_slots; t++) {
					const Reservation &reservation_tx_next = table_me->getReservation(offset + t);
					const Reservation &reservation_rx_next = table_you->getReservation(offset + t);
					CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX_CONT, reservation_tx_next.getAction());
					CPPUNIT_ASSERT_EQUAL(partner_id, reservation_tx_next.getTarget());
					CPPUNIT_ASSERT_EQUAL(true, reservation_rx_next.isRx() || reservation_rx_next.isRxCont());
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
			rlc_layer_you->should_there_be_more_p2p_data = false;
			unsigned int expected_tx_timeout = lm_me->default_timeout - 1;
			CPPUNIT_ASSERT(lm_me->current_link_state != nullptr);
			CPPUNIT_ASSERT_EQUAL(expected_tx_timeout, lm_me->current_link_state->timeout);
			// Now increment time until the link expires.
			size_t num_slots = 0, max_num_slots = lm_me->default_timeout * lm_me->burst_offset + lm_me->burst_offset;
			while (lm_me->link_status != LinkManager::link_not_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_you->link_status);
			for (const auto& channel : mac_layer_me->getReservationManager()->getP2PFreqChannels()) {
				const ReservationTable *table_me = mac_layer_me->getReservationManager()->getReservationTable(channel),
						*table_you = mac_layer_you->getReservationManager()->getReservationTable(channel);
				for (size_t t = 1; t < planning_horizon; t++) {
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table_me->getReservation(t));
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table_you->getReservation(t));
				}
			}
		}

		void testLinkExpiryMultiSlot() {
			// Update traffic estimate s.t. multi-slot bursts should be used.
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			lm_me->outgoing_traffic_estimate.put(expected_num_slots * bits_per_slot);
			unsigned int required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// Now do the other tests.
			testLinkExpiry();
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
			// Do three renewal attempts.
			 CPPUNIT_ASSERT_EQUAL(uint32_t(3), lm_me->num_renewal_attempts);
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (lm_you->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			size_t num_pending_requests = lm_me->current_link_state->scheduled_link_requests.size();
			// Proceed to the burst *before* the request is sent.
			unsigned int earliest_request_offset = 10000;
			for (const auto &item : lm_me->current_link_state->scheduled_link_requests)
				if (item.getRemainingOffset() < earliest_request_offset)
					earliest_request_offset = item.getRemainingOffset();
//			coutd.setVerbose(true);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), num_pending_requests);
			while (earliest_request_offset > lm_you->burst_offset) {
				for (size_t t = 0; t < lm_you->burst_offset; t++) {
					mac_layer_me->update(1);
					mac_layer_you->update(1);
					mac_layer_me->execute();
					mac_layer_you->execute();
					mac_layer_me->onSlotEnd();
					mac_layer_you->onSlotEnd();
					mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
					earliest_request_offset--;
				}
			}
			// Make sure it hasn't been sent yet.
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			// *Drop* the next packet.
			phy_layer_me->connected_phy = nullptr;
			phy_layer_you->connected_phy = nullptr;
			// Proceed to the request slot.
			for (size_t t = 0; t < lm_me->burst_offset; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			// Proceed to the next request.
			num_pending_requests = lm_me->current_link_state->scheduled_link_requests.size();
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts - 1), num_pending_requests);
			earliest_request_offset = 10000;
			for (const auto &item : lm_me->current_link_state->scheduled_link_requests)
				if (item.getRemainingOffset() < earliest_request_offset)
					earliest_request_offset = item.getRemainingOffset();
			while (earliest_request_offset > 0) {
				for (size_t t = 0; t < lm_you->burst_offset; t++) {
					mac_layer_me->update(1);
					mac_layer_you->update(1);
					mac_layer_me->execute();
					mac_layer_you->execute();
					mac_layer_me->onSlotEnd();
					mac_layer_you->onSlotEnd();
					mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
					earliest_request_offset--;
				}
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_me->current_link_state->scheduled_link_requests.size());
			// Reconnect.
			phy_layer_me->connected_phy = phy_layer_you;
			phy_layer_you->connected_phy = phy_layer_me;
			// Last request should be received.
			num_pending_requests = lm_me->current_link_state->scheduled_link_requests.size();
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts - 2), num_pending_requests);
			while (lm_me->current_link_state->scheduled_link_requests.size() == num_pending_requests) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			// ALl requests are sent.
			CPPUNIT_ASSERT_EQUAL(true, lm_me->current_link_state->scheduled_link_requests.empty());
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_me->link_status);
			// Proceed until reply is sent.
			for (size_t t = 0; t < lm_me->burst_offset * 2; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			CPPUNIT_ASSERT_EQUAL(*lm_me->current_channel, *lm_you->current_channel);
			CPPUNIT_ASSERT_EQUAL(false, lm_me->current_link_state->renewal_due);
			CPPUNIT_ASSERT_EQUAL(false, lm_you->current_link_state->renewal_due);
		}

		void testLinkExpiringAndLostRequestMultiSlot() {
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			num_outgoing_bits = expected_num_slots * bits_per_slot;
			lm_me->outgoing_traffic_estimate.put(num_outgoing_bits);
			unsigned int required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// Now do the other tests.
			testLinkExpiringAndLostRequest();
			required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
		}

		/**
		 * Link timeout threshold is reached.
		 * Ensures that a 2nd request is sent if the first one's reply was dropped, and that reservations made for this first request are cleared once the second one is taken.
		 */
		void testLinkExpiringAndLostReply() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			// Do three renewal attempts.
			CPPUNIT_ASSERT_EQUAL(uint32_t(3), lm_me->num_renewal_attempts);
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (lm_you->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			// Proceed to burst *before* request is sent.
			unsigned int earliest_request_offset = 10000;
			for (const auto &item : lm_me->current_link_state->scheduled_link_requests)
				if (item.getRemainingOffset() < earliest_request_offset)
					earliest_request_offset = item.getRemainingOffset();
			while (earliest_request_offset > lm_you->burst_offset) {
				for (size_t t = 0; t < lm_you->burst_offset; t++) {
					mac_layer_me->update(1);
					mac_layer_you->update(1);
					mac_layer_me->execute();
					mac_layer_you->execute();
					mac_layer_me->onSlotEnd();
					mac_layer_you->onSlotEnd();
					mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
					earliest_request_offset--;
				}
			}
			// Make sure it hasn't been sent yet.
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			// *Drop* the next reply.
			phy_layer_you->connected_phy = nullptr;
			// Proceed to the request slot.
			for (size_t t = 0; t < lm_me->burst_offset; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts - 1), lm_me->current_link_state->scheduled_link_requests.size());
			// Proceed to the next request.
			earliest_request_offset = 10000;
			for (const auto &item : lm_me->current_link_state->scheduled_link_requests)
				if (item.getRemainingOffset() < earliest_request_offset)
					earliest_request_offset = item.getRemainingOffset();
			while (earliest_request_offset > 0) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
				earliest_request_offset--;
			}
			// Proceed to reply, which is still lost.
			for (size_t t = 0; t < lm_me->burst_offset; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_me->current_link_state->scheduled_link_requests.size());
			// Reconnect -> last reply should be received.
			phy_layer_you->connected_phy = phy_layer_me;
			num_slots = 0;
			while (lm_you->link_status != LinkManager::Status::link_renewal_complete_local && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			num_slots = 0;
			while (!lm_me->current_link_state->scheduled_link_requests.empty() && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
//			coutd.setVerbose(false);

			// ALl requests are sent.
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT(lm_me->current_link_state != nullptr);
			CPPUNIT_ASSERT_EQUAL(true, lm_me->current_link_state->scheduled_link_requests.empty());
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete_local, lm_you->link_status);
//			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_you->current_link_state->scheduled_link_replies.size());
			// Proceed until reply is sent.
			num_slots = 0;
			while (lm_you->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT(lm_me->current_link_state != nullptr);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			CPPUNIT_ASSERT_EQUAL(*lm_me->current_channel, *lm_you->current_channel);
			CPPUNIT_ASSERT_EQUAL(false, lm_me->current_link_state->renewal_due);
			CPPUNIT_ASSERT_EQUAL(false, lm_you->current_link_state->renewal_due);

			coutd << "LINK RENEWAL COMPLETE" << std::endl;

			while (lm_me->current_reservation_table->getReservation(0).isIdle()) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}

			// Reservations made at TX and RX from earlier requests should've been cleared.
			size_t num_res_tx = 0, num_res_rx = 0, num_res_additional_tx = 0, num_res_additional_rx = 0;
			unsigned int expiry_at = lm_me->current_link_state->timeout*lm_me->burst_offset + lm_me->current_link_state->burst_length;
			for (size_t t = 0; t < expiry_at * 2; t++) {
				for (const FrequencyChannel* channel : lm_me->reservation_manager->getP2PFreqChannels()) {
					const Reservation& res_tx = lm_me->reservation_manager->getReservationTable(channel)->getReservation(t);
					const Reservation& res_rx = lm_you->reservation_manager->getReservationTable(channel)->getReservation(t);
					coutd << "t=" << t << " f=" << *channel << ": " << res_tx << " | " << res_rx << std::endl;
					if (*channel == *lm_me->current_channel) {
						if (!res_tx.isIdle()) {
							CPPUNIT_ASSERT(res_rx == Reservation(own_id, Reservation::RX) || res_rx == Reservation(own_id, Reservation::RX_CONT));
							// Only count burst starts...
							if (res_rx == Reservation(own_id, Reservation::RX)) {
								CPPUNIT_ASSERT(t % lm_me->burst_offset == 0);
								num_res_tx++;
							}
						}
						if (!res_rx.isIdle()) {
							if (!(res_tx == Reservation(partner_id, Reservation::TX) || res_tx == Reservation(partner_id, Reservation::TX_CONT)))
								std::cout << res_tx << std::endl;
							CPPUNIT_ASSERT(res_tx == Reservation(partner_id, Reservation::TX) || res_tx == Reservation(partner_id, Reservation::TX_CONT));
							if (res_tx == Reservation(partner_id, Reservation::TX)) {
								CPPUNIT_ASSERT(t % lm_me->burst_offset == 0);
								num_res_rx++;
							}
						}
					} else {
						if (!res_tx.isIdle())
							num_res_additional_tx++;
						if (!res_rx.isIdle())
							num_res_additional_rx++;
					}
				}
			}
			size_t expected_num_res = lm_me->default_timeout;
			CPPUNIT_ASSERT_EQUAL(expected_num_res, num_res_tx);
			CPPUNIT_ASSERT_EQUAL(expected_num_res, num_res_rx);
			// No reservations on other channels should exist.
			CPPUNIT_ASSERT_EQUAL(size_t(0), num_res_additional_tx);
			CPPUNIT_ASSERT_EQUAL(size_t(0), num_res_additional_rx);
		}

		/** Tests bug I couldn't fix without its own test: resources locked after sending one lost renewal request weren't properly free'd when another had to be sent. */
		void testLockedResourcesFreedOnSecondReplyArrival() {
			// Multi-slot reservations.
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			num_outgoing_bits = expected_num_slots * bits_per_slot;
			lm_me->outgoing_traffic_estimate.put(num_outgoing_bits);
			unsigned int required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (lm_you->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			// Disconnect.
			phy_layer_me->connected_phy = nullptr;
			// Send and lose first renewal reply.
			size_t earliest_request_offset = 10000;
			for (const auto &msg : lm_me->current_link_state->scheduled_link_requests) {
				if (msg.getRemainingOffset() < earliest_request_offset)
					earliest_request_offset = msg.getRemainingOffset();
			}
			CPPUNIT_ASSERT(earliest_request_offset < 10000);
			// Proceed to one slot before request is sent.
			for (size_t t = 0; t < earliest_request_offset - 1; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			// Turn on output.
//			coutd.setVerbose(true);
			// Send request, which is lost.
			mac_layer_me->update(1);
			mac_layer_you->update(1);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts - 1), lm_me->current_link_state->scheduled_link_requests.size());
			CPPUNIT_ASSERT_EQUAL(true, lm_me->current_link_state->renewal_due);
			// There should be some resources locked from sending the renewal request.
			size_t num_locked = 0;
			coutd << "Locked: " << std::endl;
			for (const auto *channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
				for (size_t t = 0; t < planning_horizon; t++) {
					if (mac_layer_me->reservation_manager->getReservationTable(channel)->getReservation(t).isLocked()) {
						coutd << "f=" << *channel << ",t=" << t << " ";
						num_locked++;
					}
				}
				coutd << std::endl;
			}
			coutd << std::endl;
			CPPUNIT_ASSERT(num_locked > 0);
			// Reconnect.
			phy_layer_me->connected_phy = phy_layer_you;
//			coutd.setVerbose(false);
			earliest_request_offset = 10000;
			for (const auto &msg : lm_me->current_link_state->scheduled_link_requests) {
				if (msg.getRemainingOffset() < earliest_request_offset)
					earliest_request_offset = msg.getRemainingOffset();
			}
			CPPUNIT_ASSERT(earliest_request_offset < 10000);
			// Proceed to one slot before request is sent.
			for (size_t t = 0; t < earliest_request_offset - 1; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			// Turn on output.
//			coutd.setVerbose(true);
			coutd << std::endl << "--- SECOND REQUEST ---" << std::endl << std::endl;
			// Send request, which is received.
			mac_layer_me->update(1);
			mac_layer_you->update(1);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts - 2), lm_me->current_link_state->scheduled_link_requests.size());
			size_t num_locked_now = 0;
			coutd << "Locked now: " << std::endl;
			for (const auto *channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
				for (size_t t = 0; t < planning_horizon; t++) {
					if (mac_layer_me->reservation_manager->getReservationTable(channel)->getReservation(t).isLocked()) {
						coutd << "f=" << *channel << ",t=" << t << " ";
						num_locked_now++;
					}
				}
				coutd << std::endl;
			}
			coutd << std::endl;
			CPPUNIT_ASSERT_EQUAL(num_locked, num_locked_now);
//			coutd.setVerbose(false);
			// Proceed up to link renewal.
			num_slots = 0;
			while (lm_you->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			// No locked resources should exist.
			for (const auto *channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
				for (size_t t = 0; t < planning_horizon; t++) {
					CPPUNIT_ASSERT_EQUAL(false, mac_layer_me->reservation_manager->getReservationTable(channel)->getReservation(t).isLocked());
				}
			}
//			coutd.setVerbose(false);
		}

		void testLinkExpiringAndLostReplyMultiSlot() {
//			coutd.setVerbose(true);
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			num_outgoing_bits = expected_num_slots * bits_per_slot;
			lm_me->outgoing_traffic_estimate.put(num_outgoing_bits);
			unsigned int required_slots = std::max(uint32_t(1), lm_me->estimateCurrentNumSlots());
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// Now do the other tests.
			testLinkExpiringAndLostReply();
			required_slots = std::max(uint32_t(1), lm_me->estimateCurrentNumSlots());
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
//			coutd.setVerbose(false);
		}

		/** Tests that reservations at both communication partners match at all times until link expiry. */
		void testReservationsUntilExpiry() {
			// Single message.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 1000;
			while (lm_you->link_status != LinkManager::link_established && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(true, lm_me->current_reservation_table->getReservation(0).isTx());
			CPPUNIT_ASSERT_EQUAL(true, lm_you->current_reservation_table->getReservation(0).isRx());

			num_slots = 0;
			while (lm_me->link_status != LinkManager::link_not_established && num_slots++ < max_slots) {
				mac_layer_me->update(lm_me->burst_offset);
				mac_layer_you->update(lm_you->burst_offset);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				if (lm_me->link_status != LinkManager::link_not_established) {
					size_t num_non_idle = 0;
					for (int t = 1; t < planning_horizon; t++) {
						const Reservation& res_tx = lm_me->current_reservation_table->getReservation(t);
						const Reservation& res_rx = lm_you->current_reservation_table->getReservation(t);
						if (res_tx.isTx()) {
							num_non_idle++;
							CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), res_tx);
							CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), res_rx);
						}
					}
					CPPUNIT_ASSERT_EQUAL(size_t(lm_me->current_link_state->timeout), num_non_idle);
				}
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_you->link_status);
		}

		void testRenewalRequest() {
			// Establish link and send first burst.
			testLinkEstablishment();
			// Renewal attempts *are* made if there's more data.
			rlc_layer_me->should_there_be_more_p2p_data = true;

			// 1st request + 1 data packet should've been sent so far.
			size_t expected_num_sent_packets = 2;
			CPPUNIT_ASSERT_EQUAL(expected_num_sent_packets, phy_layer_me->outgoing_packets.size());

//			coutd.setVerbose(true);
			auto *link_manager = (P2PLinkManager*) mac_layer_me->getLinkManager(partner_id);
			size_t num_slots = 0, max_slots = 10000, initial_reqs_to_send = link_manager->current_link_state->scheduled_link_requests.size();
			// Increment time to each request slot...
			while (num_slots++ < max_slots && link_manager->current_link_state->scheduled_link_requests.size() == initial_reqs_to_send) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			L2Packet* request = phy_layer_me->outgoing_packets.at(phy_layer_me->outgoing_packets.size() - 1);
			CPPUNIT_ASSERT(request->getHeaders().size() >= 2);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request,request->getHeaders().at(1)->frame_type);
			// Current slot should have been used to transmit the request.
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, link_manager->current_reservation_table->getReservation(0).getAction());
			// And next burst to receive the reply.
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, link_manager->current_reservation_table->getReservation(link_manager->burst_offset).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, mac_layer_you->getLinkManager(own_id)->current_reservation_table->getReservation(link_manager->burst_offset).getAction());
			CPPUNIT_ASSERT_EQUAL(initial_reqs_to_send - 1, link_manager->current_link_state->scheduled_link_requests.size());
		}

		void testLinkRenewal() {
			// Proceed to a renewal having been sent.
//			coutd.setVerbose(true);
			testRenewalRequest();
			CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_renewal_complete_local, lm_you->link_status);
			size_t num_slots = 0, max_slots = 10000;
			while (lm_me->link_status != LinkManager::link_established && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_you->link_status);

			ReservationTable *tbl_tx = lm_me->current_reservation_table, *tbl_rx = lm_you->current_reservation_table;
			size_t num_non_idle = 0;
			for (unsigned int t = 0; t < planning_horizon; t++) {
				const Reservation &res_tx = tbl_tx->getReservation(t);
				const Reservation &res_rx = tbl_rx->getReservation(t);
				if (res_tx == Reservation(partner_id, Reservation::TX)) {
					CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), res_rx);
					num_non_idle++;
				} else {
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res_rx);
				}
			}
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->default_timeout), num_non_idle);

			// TODO check ohter channels
		}

		/**
		 * Tests link renewal with a channel change.
		 */
		void testLinkRenewalChannelChange() {
//			coutd.setVerbose(true);
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = lm_me->default_timeout * (lm_me->burst_offset * 2);
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (lm_you->link_status != P2PLinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			// Link establishment should've worked.
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_you->link_status);
			// One data transmission should've happened (which established the link at RX).
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout - 1, lm_you->current_link_state->timeout);

			// Proceed until the first request.
			CPPUNIT_ASSERT(lm_me->num_renewal_attempts > 0);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			num_slots = 0;
			while (lm_me->current_link_state->scheduled_link_requests.size() > lm_me->num_renewal_attempts - 1 && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(true, lm_me->current_link_state->renewal_due);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(true, lm_you->current_link_state->renewal_due);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_renewal_complete_local, lm_you->link_status);

			// Proceed until the reply has been received.
			num_slots = 0;
			while ((lm_me->link_status != P2PLinkManager::link_renewal_complete) && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_renewal_complete, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_renewal_complete_local, lm_you->link_status);
			// Channel change (comes from proposing two channels by default, out of three, and the most idle ones are used <-> won't reuse the current channel).
			CPPUNIT_ASSERT(*lm_me->next_link_state->channel != *lm_me->current_channel);
			// They should agree on the new channel.
			CPPUNIT_ASSERT(*lm_me->next_link_state->channel == *lm_you->next_link_state->channel);

			// Proceed until the link is renewed.
			num_slots = 0;
			while ((lm_me->link_status != P2PLinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(*lm_me->current_channel, *lm_you->current_channel);
			CPPUNIT_ASSERT(lm_me->next_link_state == nullptr);
			CPPUNIT_ASSERT(lm_you->next_link_state == nullptr);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->current_link_state->scheduled_link_requests.size());

			// Proceed until first reservation of new link.
			do {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			} while (lm_me->current_reservation_table->getReservation(0).isIdle());

			// Make sure that all requests target correct slots.
			for (size_t t = 0; t <= lm_me->getExpiryOffset(); t++) {
				// Make sure there's no reservations on any other channel for both TX and RX.
				size_t num_res_other_channels_tx = 0, num_res_other_channels_rx = 0;
				for (const auto* channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
					if (*channel != *lm_me->current_channel) {
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

				const Reservation& reservation_tx = lm_me->current_reservation_table->getReservation(t);
				const Reservation& reservation_rx = lm_you->current_reservation_table->getReservation(t);
				coutd << "t=" << t << ": " << reservation_tx << "|" << reservation_rx;
				if (std::any_of(lm_me->current_link_state->scheduled_link_requests.begin(), lm_me->current_link_state->scheduled_link_requests.end(), [t](P2PLinkManager::ControlMessageReservation& t_request){
					return t_request.getRemainingOffset() == t;
				})) {
					coutd << " REQUEST";
					// Request slots should match TX slots.
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), reservation_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), lm_you->current_reservation_table->getReservation(t));
				}
				coutd << std::endl;
				// Matching reservations.
				if (t % lm_me->burst_offset == 0) {
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), reservation_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), lm_you->current_reservation_table->getReservation(t));
				}
			}

//			coutd.setVerbose(true);

			// Proceed until the next renewal request has been sent.
			num_slots = 0;
			while (lm_me->current_link_state->scheduled_link_requests.size() > lm_me->num_renewal_attempts - 1 && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), lm_me->current_reservation_table->getReservation(lm_me->burst_offset + lm_me->current_link_state->burst_length - 1));
			CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::TX), lm_you->current_reservation_table->getReservation(lm_me->burst_offset + lm_me->current_link_state->burst_length - 1));
			// Proceed until the reply has been sent.
			size_t n = lm_me->burst_offset + lm_me->current_link_state->burst_length - 1;
			for (size_t i = 0; i < n; i++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_renewal_complete, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_renewal_complete_local, lm_you->link_status);
			// Proceed until the link is renewed.
			num_slots = 0;
			while ((lm_me->link_status != P2PLinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(*lm_me->current_channel, *lm_you->current_channel);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->current_link_state->scheduled_link_requests.size());

//			coutd.setVerbose(false);
		}

		void testLinkRenewalChannelChangeMultiSlot() {
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			num_outgoing_bits = expected_num_slots * bits_per_slot;
			lm_me->outgoing_traffic_estimate.put(num_outgoing_bits);
			unsigned int required_slots = std::max(uint32_t(1), lm_me->estimateCurrentNumSlots());
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// Now do the other tests.
//			coutd.setVerbose(true);
			testLinkRenewalChannelChange();
			required_slots = std::max(uint32_t(1), lm_me->estimateCurrentNumSlots());
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			size_t num_tx_me = 0, num_tx_cont_me = 0, num_rx_you = 0, num_rx_cont_you = 0;
			for (const auto& channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
				const ReservationTable *table_me = mac_layer_me->getReservationManager()->getReservationTable(channel),
						*table_you = mac_layer_you->getReservationManager()->getReservationTable(channel);
				for (size_t t = 0; t < lm_me->burst_offset; t++) {
					const Reservation& res_me = table_me->getReservation(t);
					if (res_me.isTx())
						num_tx_me++;
					else if (res_me.isTxCont())
						num_tx_cont_me++;
					else
						CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res_me);

					const Reservation& res_you = table_you->getReservation(t);
					if (res_you.isRx())
						num_rx_you++;
					else if (res_you.isRxCont())
						num_rx_cont_you++;
					else
						CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res_you);
				}
			}
			CPPUNIT_ASSERT(num_tx_me > 0);
			CPPUNIT_ASSERT_EQUAL(num_tx_me, num_rx_you);
			CPPUNIT_ASSERT(num_tx_cont_me > 0);
			CPPUNIT_ASSERT_EQUAL(num_tx_cont_me, num_rx_cont_you);
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
			lm_me->num_p2p_channels_to_propose = 1;
			lm_you->num_p2p_channels_to_propose = 1;
//			coutd.setVerbose(true);

			// Do link establishment.
			size_t num_slots = 0;
			const size_t max_num_slots = lm_me->default_timeout * (lm_me->burst_offset*2);
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (lm_you->link_status != P2PLinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			// Link establishment should've worked.
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_you->link_status);
			// One data transmission should've happened (which established the link at RX).
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout - 1, lm_you->current_link_state->timeout);

			// Proceed until the first request.
			CPPUNIT_ASSERT(lm_me->num_renewal_attempts > 0);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			num_slots = 0;
			while (lm_me->current_link_state->scheduled_link_requests.size() > lm_me->num_renewal_attempts - 1 && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(true, lm_me->current_link_state->renewal_due);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(true, lm_you->current_link_state->renewal_due);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_renewal_complete_local, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(lm_me->current_link_state->timeout, lm_you->current_link_state->timeout);

			// Proceed until the reply has been received.
			num_slots = 0;
			while ((lm_me->link_status != P2PLinkManager::link_renewal_complete) && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
				for (size_t t = 0; t < planning_horizon; t++) {
					const Reservation &res_tx = lm_me->current_reservation_table->getReservation(t), &res_rx = lm_you->current_reservation_table->getReservation(t);
					if (res_tx.isTx())
						CPPUNIT_ASSERT_EQUAL(true, res_rx.isRx());
					else if (res_tx.isRx())
						CPPUNIT_ASSERT_EQUAL(true, res_rx.isTx());
				}
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(lm_me->current_link_state->timeout, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_renewal_complete, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_renewal_complete_local, lm_you->link_status);
			// No channel change.
			CPPUNIT_ASSERT(*lm_me->next_link_state->channel == *lm_me->current_channel);
			// They should agree on the new channel.
			CPPUNIT_ASSERT(*lm_me->next_link_state->channel == *lm_you->next_link_state->channel);
			for (size_t t = 0; t < planning_horizon; t++) {
				const Reservation &res_tx = lm_me->current_reservation_table->getReservation(t), &res_rx = lm_you->current_reservation_table->getReservation(t);
				if (res_tx.isTx())
					CPPUNIT_ASSERT_EQUAL(true, res_rx.isRx());
				else if (res_tx.isRx())
					CPPUNIT_ASSERT_EQUAL(true, res_rx.isTx());
			}

//			coutd.setVerbose(true);

			// Proceed until the link is renewed.
			num_slots = 0;
			while ((lm_me->link_status != P2PLinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
				const Reservation &res_tx = lm_me->current_reservation_table->getReservation(0), &res_rx = lm_you->current_reservation_table->getReservation(0);
				if (!res_tx.isIdle() && !res_tx.isLocked())
					CPPUNIT_ASSERT_EQUAL(false, res_rx.isIdle());
				else if (res_tx.isIdle())
					CPPUNIT_ASSERT_EQUAL(true, res_rx.isIdle());
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			for (size_t t = 0; t < planning_horizon; t++) {
				const Reservation &res_tx = lm_me->current_reservation_table->getReservation(t), &res_rx = lm_you->current_reservation_table->getReservation(t);
//				coutd.setVerbose(true);
//				coutd << "t=" << t << " tx=" << res_tx << "|" << res_rx << std::endl;
//				coutd.setVerbose(false);
				if (res_tx.isTx())
					CPPUNIT_ASSERT_EQUAL(true, res_rx.isRx());
				else if (res_tx.isRx())
					CPPUNIT_ASSERT_EQUAL(true, res_rx.isTx());
			}
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(*lm_me->current_channel, *lm_you->current_channel);
			CPPUNIT_ASSERT(lm_me->next_link_state == nullptr);
			CPPUNIT_ASSERT(lm_you->next_link_state == nullptr);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->current_link_state->scheduled_link_requests.size());

			// Proceed until first reservation of new link.
			do {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			} while (lm_me->current_reservation_table->getReservation(0).isIdle() || lm_me->current_reservation_table->getReservation(0).isTxCont());

			// Make sure that all requests target correct slots.
			for (size_t t = 0; t <= lm_me->getExpiryOffset(); t++) {
				// Make sure there's no reservations on any other channel for both TX and RX.
				size_t num_res_other_channels_tx = 0, num_res_other_channels_rx = 0;
				for (const auto* channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
					if (*channel != *lm_me->current_channel) {
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

				const Reservation& reservation_tx = lm_me->current_reservation_table->getReservation(t);
				const Reservation& reservation_rx = lm_you->current_reservation_table->getReservation(t);
				coutd << "t=" << t << ": " << reservation_tx << "|" << reservation_rx;
				if (std::any_of(lm_me->current_link_state->scheduled_link_requests.begin(), lm_me->current_link_state->scheduled_link_requests.end(), [t](P2PLinkManager::ControlMessageReservation& t_request){
					return t_request.getRemainingOffset() == t;
				})) {
					coutd << " REQUEST";
					// Request slots should match TX slots.7
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), reservation_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), lm_you->current_reservation_table->getReservation(t));
				}
				coutd << std::endl;
				// Matching reservations.
				unsigned int burst_length = lm_me->current_link_state->burst_length;
				if (t % lm_me->burst_offset == 0) {
					for (size_t i = 0; i < burst_length; i++) {
						coutd << "i=" << i << ": " << lm_me->current_reservation_table->getReservation(t + i) << std::endl;
						if (i == 0) {
							CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), reservation_tx);
							CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), reservation_rx);
						} else {
							CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX_CONT), lm_me->current_reservation_table->getReservation(t + i));
							CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX_CONT), lm_you->current_reservation_table->getReservation(t + i));
						}
					}
				}
			}

//			coutd.setVerbose(true);

			// Proceed until the next renewal has been negotiated.
			num_slots = 0;
			while (lm_me->current_link_state->scheduled_link_requests.size() > lm_me->num_renewal_attempts - 1 && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), lm_me->current_reservation_table->getReservation(lm_me->burst_offset + lm_me->current_link_state->burst_length - 1));
			CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::TX), lm_you->current_reservation_table->getReservation(lm_me->burst_offset + lm_me->current_link_state->burst_length - 1));
			// Proceed until the reply has been sent.
			for (auto t = 0; t < lm_me->burst_offset + lm_me->current_link_state->burst_length - 1; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_renewal_complete, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_renewal_complete_local, lm_you->link_status);
			// Proceed until the link is renewed.
			num_slots = 0;
			while ((lm_me->link_status != P2PLinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(*lm_me->current_channel, *lm_you->current_channel);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(lm_me->num_renewal_attempts), lm_me->current_link_state->scheduled_link_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->current_link_state->scheduled_link_requests.size());
//			coutd.setVerbose(false);
		}

		void testLinkRenewalSameChannelMultiSlot() {
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			num_outgoing_bits = expected_num_slots * bits_per_slot;
			lm_me->outgoing_traffic_estimate.put(num_outgoing_bits);
			unsigned int required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// Now do the other tests.
			testLinkRenewalSameChannel();
		}

		/**
		 * Link timeout threshold is reached.
		 * Ensures that if no negotiation has happened prior to expiry, the link is reset and attempted to be reestablished.
		 */
		void testLinkRenewalFails() {
//			coutd.setVerbose(true);
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			while (lm_you->link_status != P2PLinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				mac_layer_me->notifyOutgoing(num_outgoing_bits, partner_id);
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout - 1, lm_you->current_link_state->timeout);

			// From now on, drop all packets.
			phy_layer_me->connected_phy = nullptr;
			phy_layer_you->connected_phy = nullptr;

			num_slots = 0;
			max_num_slots = lm_me->current_link_state->timeout * lm_me->burst_offset + lm_me->burst_offset;
			while (lm_you->link_status != P2PLinkManager::link_not_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_not_established, lm_you->link_status);
			CPPUNIT_ASSERT(lm_me->current_channel == nullptr);
			CPPUNIT_ASSERT(lm_me->current_reservation_table == nullptr);
			CPPUNIT_ASSERT(lm_you->current_channel == nullptr);
			CPPUNIT_ASSERT(lm_you->current_reservation_table == nullptr);

			for (auto channel : mac_layer_me->reservation_manager->getP2PFreqChannels()) {
				for (size_t t = lm_me->burst_offset; t < planning_horizon; t++) {
					const Reservation& res_tx = mac_layer_me->getReservationManager()->getReservationTable(channel)->getReservation(t);
					const Reservation& res_rx = mac_layer_you->getReservationManager()->getReservationTable(channel)->getReservation(t);
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res_tx);
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res_rx);
				}
			}
		}

		void testLinkRenewalFailsMultiSlot() {
			unsigned long bits_per_slot = phy_layer_me->getCurrentDatarate();
			unsigned int expected_num_slots = 3;
			num_outgoing_bits = expected_num_slots * bits_per_slot;
			lm_me->outgoing_traffic_estimate.put(num_outgoing_bits);
			unsigned int required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// Now do the other tests.
			testLinkRenewalFails();
			required_slots = lm_me->estimateCurrentNumSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
		}

		void testLinkRenewalAfterExpiry() {
			testLinkRenewalFails();
			// Reconnect.
			phy_layer_me->connected_phy = phy_layer_you;
			phy_layer_you->connected_phy = phy_layer_me;
			// Now try link establishment again.
//			coutd.setVerbose(true);
			size_t num_slots = 0, max_num_slots = 100;
			auto *lm_tx = (P2PLinkManager*) mac_layer_me->getLinkManager(partner_id),
					*lm_rx = (P2PLinkManager*) mac_layer_you->getLinkManager(own_id);
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (lm_rx->link_status != P2PLinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_established, lm_tx->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::link_established, lm_rx->link_status);
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
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (lm_me->link_status != P2PLinkManager::Status::link_renewal_complete && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Proceed to one more data transmission.
			mac_layer_me->update(lm_me->burst_offset);
			mac_layer_you->update(lm_me->burst_offset);
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
			mac_layer_me->notifyOutgoing(1024, partner_id);
		}

		/** Before introducing the onSlotEnd() function, success depended on the order of the execute() calls (which is of course terrible),
		 * so this test ensures that the order in which user actions are executed doesn't matter.
		 */
		void testCommunicateInOtherDirection() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			auto *lm_tx = (P2PLinkManager*) mac_layer_me->getLinkManager(partner_id),
					*lm_rx = (P2PLinkManager*) mac_layer_you->getLinkManager(own_id);
			// Other guy tries to communicate with us.
			mac_layer_you->notifyOutgoing(512, own_id);
			while (lm_tx->link_status != P2PLinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_rx->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_tx->link_status);
		}

		/** Before introducing the onSlotEnd() function, success depended on the order of the execute() calls (which is of course terrible),
		 * so this test ensures that the order in which user actions are executed doesn't matter.
		 */
		void testCommunicateReverseOrder() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;
			auto *lm_tx = (P2PLinkManager*) mac_layer_me->getLinkManager(partner_id),
					*lm_rx = (P2PLinkManager*) mac_layer_you->getLinkManager(own_id);
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (lm_rx->link_status != P2PLinkManager::Status::link_established && num_slots++ < max_num_slots) {
				// you first, then me
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_rx->link_status);
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, lm_tx->link_status);
		}

		void testPacketSize() {
			testLinkEstablishment();
			CPPUNIT_ASSERT_EQUAL(size_t(2), phy_layer_me->outgoing_packets.size());
			L2Packet *packet = phy_layer_me->outgoing_packets.at(1);
			CPPUNIT_ASSERT_EQUAL(phy_layer_me->getCurrentDatarate(), (unsigned long) packet->getBits());
		}

		void testReportedTxSlotDesire() {
			// Should schedule 1 TX slot each.
			lm_me->reported_desired_tx_slots = 1;
			// Single message.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 100;
			while (lm_you->link_status != LinkManager::Status::link_established && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->current_link_state->timeout, lm_you->current_link_state->timeout);
			mac_layer_me->update(1);
			mac_layer_you->update(1);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			for (size_t t = 0; t < lm_me->burst_offset - 2; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}

			// Now we're at the first proper burst with both sides transmitting.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->current_link_state->timeout, lm_you->current_link_state->timeout);
//			coutd.setVerbose(true);
			CPPUNIT_ASSERT_EQUAL(true, lm_me->current_reservation_table->getReservation(1).isTx());
			CPPUNIT_ASSERT_EQUAL(true, lm_you->current_reservation_table->getReservation(1).isRx());
			CPPUNIT_ASSERT_EQUAL(true, lm_me->current_reservation_table->getReservation(2).isRx());
			CPPUNIT_ASSERT_EQUAL(true, lm_you->current_reservation_table->getReservation(2).isTx());
			// Execute first slot with *me* transmitting.
			mac_layer_me->update(1);
			mac_layer_you->update(1);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 2, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->current_link_state->timeout, lm_you->current_link_state->timeout);
			// Execute second slot with *you* transmitting.
			mac_layer_me->update(1);
			mac_layer_you->update(1);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			// Which *shouldn't* have decremented the timeout again.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 2, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->current_link_state->timeout, lm_you->current_link_state->timeout);
		}

		/** Tests that if a renewal request is processed, but the reply doesn't make it, there's not one partner that renews the link while another does not. */
		void testUnSyncedRenewal() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 100;

			// Establish link.
			while (lm_me->link_status != LinkManager::Status::link_established && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// Send request.
			num_slots = 0;
			while (lm_me->current_link_state->scheduled_link_requests.size() > lm_me->num_renewal_attempts - 1 && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}

			phy_layer_you->connected_phy = nullptr;
			// Proceed until expiry.
			size_t expiry_slot = lm_me->getExpiryOffset() + lm_me->burst_offset - 1;
//			coutd.setVerbose(true);
			for (size_t t = 0; t < expiry_slot; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}

			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, lm_you->link_status);
		}


	CPPUNIT_TEST_SUITE(NewSystemTests);
			CPPUNIT_TEST(testLinkEstablishment);
			CPPUNIT_TEST(testLinkEstablishmentMultiSlotBurst);
			CPPUNIT_TEST(testLinkExpiry);
			CPPUNIT_TEST(testLinkExpiryMultiSlot);
			CPPUNIT_TEST(testLinkExpiringAndLostRequest);
			CPPUNIT_TEST(testLinkExpiringAndLostRequestMultiSlot);
			CPPUNIT_TEST(testLinkExpiringAndLostReply);
			CPPUNIT_TEST(testLinkExpiringAndLostReplyMultiSlot);
			CPPUNIT_TEST(testLockedResourcesFreedOnSecondReplyArrival);
			CPPUNIT_TEST(testReservationsUntilExpiry);
			CPPUNIT_TEST(testRenewalRequest);
			CPPUNIT_TEST(testLinkRenewal);
			CPPUNIT_TEST(testLinkRenewalChannelChange);
			CPPUNIT_TEST(testLinkRenewalChannelChangeMultiSlot);
			CPPUNIT_TEST(testLinkRenewalSameChannel);
			CPPUNIT_TEST(testLinkRenewalSameChannelMultiSlot);
			CPPUNIT_TEST(testLinkRenewalFails);
			CPPUNIT_TEST(testLinkRenewalFailsMultiSlot);
			CPPUNIT_TEST(testLinkRenewalAfterExpiry);
			CPPUNIT_TEST(testSimulatorScenario);
			CPPUNIT_TEST(testCommunicateInOtherDirection);
			CPPUNIT_TEST(testCommunicateReverseOrder);
			CPPUNIT_TEST(testPacketSize);
			CPPUNIT_TEST(testReportedTxSlotDesire);
			CPPUNIT_TEST(testUnSyncedRenewal);
		CPPUNIT_TEST_SUITE_END();
	};
}