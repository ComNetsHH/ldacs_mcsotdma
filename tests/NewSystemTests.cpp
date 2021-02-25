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
			auto* lm_me = (P2PLinkManager*) mac_layer_me->getLinkManager(partner_id);
			auto* lm_you = (P2PLinkManager*) mac_layer_you->getLinkManager(own_id);
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
		 * Tests that a link expires when the timeout is reached.
		 */
		void testLinkExpiry() {
			// Establish link and send first burst.
			testLinkEstablishment();
			// Don't try to renew the link.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			auto *lm_me = (P2PLinkManager*) mac_layer_me->getLinkManager(partner_id);
			auto *lm_you = (P2PLinkManager*) mac_layer_you->getLinkManager(own_id);
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
			for (size_t t = 0; t < lm_me->burst_offset; t++) {
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

		/** Tests that reservations at both communication partners match at all times until link expiry. */
		void testReservationsUntilExpiry() {
			// Single message.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, partner_id);
			auto* lm_me = (P2PLinkManager*) mac_layer_me->getLinkManager(partner_id);
			auto* lm_you = (P2PLinkManager*) mac_layer_you->getLinkManager(own_id);
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
			testRenewalRequest();
//			coutd.setVerbose(true);
			CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_renewal_complete, lm_you->link_status);
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
		}


	CPPUNIT_TEST_SUITE(NewSystemTests);
			CPPUNIT_TEST(testLinkEstablishment);
			CPPUNIT_TEST(testLinkExpiry);
			CPPUNIT_TEST(testLinkExpiringAndLostRequest);
			CPPUNIT_TEST(testReservationsUntilExpiry);
			CPPUNIT_TEST(testRenewalRequest);
			CPPUNIT_TEST(testLinkRenewal);
		CPPUNIT_TEST_SUITE_END();
	};
}