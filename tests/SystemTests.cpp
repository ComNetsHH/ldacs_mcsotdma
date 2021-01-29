//
// Created by Sebastian Lindner on 10.12.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"
#include "../BCLinkManager.hpp"
#include "../LinkManagementEntity.hpp"


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
			rlc_layer_me->should_there_be_more_data = false;
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
			rlc_layer_me->should_there_be_more_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			size_t num_slots = 0, max_slots = 20;
			LinkManager* lm_me = mac_layer_me->getLinkManager(communication_partner_id);
			LinkManager* lm_you = mac_layer_you->getLinkManager(own_id);
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->statistic_num_received_packets);
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->statistic_num_received_packets);
			while (((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->broadcast_slot_scheduled && num_slots++ < max_slots) {
				// Order is important: if 'you' updates last, the reply may already be sent, and we couldn't check the next condition (or check for both 'awaiting_reply' OR 'established').
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
			CPPUNIT_ASSERT_EQUAL(size_t(0), mac_layer_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->statistic_num_received_packets);
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
//                coutd.setVerbose(true);
			// Single message.
			rlc_layer_me->should_there_be_more_data = false;
			LinkManager* lm_me = mac_layer_me->getLinkManager(communication_partner_id);
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
				required_slots = lm_me->estimateCurrentNumSlots();
				CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			}
			// Ensure that the request requested a multi-slot reservation.
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
			L2Packet* request = rlc_layer_you->receptions.at(0);
			CPPUNIT_ASSERT_EQUAL(size_t(2), request->getHeaders().size());
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, request->getHeaders().at(1)->frame_type);
			required_slots = lm_me->estimateCurrentNumSlots();
			auto* request_payload = (LinkManagementEntity::ProposalPayload*) request->getPayloads().at(1);
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
			}
			// Link reply should've arrived, so *our* link should be established...
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_me->getLinkManager(communication_partner_id)->link_establishment_status);
			// ... and *their* link should indicate that the reply has been sent.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_data_tx, mac_layer_you->getLinkManager(own_id)->link_establishment_status);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->lme->default_tx_timeout, lm_me->lme->tx_timeout);
			// Make sure that all corresponding slots are marked as TX on our side.
			ReservationTable* table_me = lm_me->current_reservation_table;
			for (int offset = lm_me->lme->tx_offset; offset < lm_me->lme->tx_timeout * lm_me->lme->tx_offset; offset += lm_me->lme->tx_offset) {
				for (int i = 0; i < expected_num_slots; i++) {
					const Reservation& reservation = table_me->getReservation(offset + i);
					if (i == 0)
						CPPUNIT_ASSERT_EQUAL(true, reservation.isTx());
					else
						CPPUNIT_ASSERT_EQUAL(true, reservation.isTxCont());
					CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation.getTarget());
				}
			}
			// Make sure that the same slots are marked as RX on their side.
			LinkManager* lm_you = mac_layer_you->getLinkManager(own_id);
			ReservationTable* table_you = lm_you->current_reservation_table;
			std::vector<int> reserved_time_slots;
			for (int offset = lm_me->lme->tx_offset; offset < lm_me->lme->tx_timeout * lm_me->lme->tx_offset; offset += lm_me->lme->tx_offset) {
				for (int i = 0; i < expected_num_slots; i++) {
					const Reservation& reservation = table_you->getReservation(offset + i);
					CPPUNIT_ASSERT_EQUAL(own_id, reservation.getTarget());
					CPPUNIT_ASSERT_EQUAL(true, reservation.isRx());
					reserved_time_slots.push_back(offset + i);
				}
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
			// Wait until the next transmission.
			for (size_t i = 0; i < lm_you->lme->tx_offset; i++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				std::pair<size_t, size_t> exes_me = mac_layer_me->execute();
				std::pair<size_t, size_t> exes_you = mac_layer_you->execute();
				// Since the link is now established, reservation tables should match:
				// The number of transmissions I send must equal the number of receptions you receive...
				CPPUNIT_ASSERT_EQUAL(exes_me.first, exes_you.second);
				// ... and vice-versa.
				CPPUNIT_ASSERT_EQUAL(exes_me.second, exes_you.first);
			}
			// *Their* status should now show an established link.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_you->getLinkManager(own_id)->link_establishment_status);
			// Reservation timeout should be 1 less now.
			CPPUNIT_ASSERT_EQUAL(lm_me->lme->default_tx_timeout - 1, lm_me->lme->tx_timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(2), rlc_layer_you->receptions.size());
			// Ensure reservations are still valid.
			for (size_t i = 0; i < reserved_time_slots.size(); i++) {
				int offset = reserved_time_slots.at(i);
				const Reservation& reservation = table_you->getReservation(offset - lm_you->lme->tx_offset); // Normalize saved offsets to current time
				CPPUNIT_ASSERT_EQUAL(own_id, reservation.getTarget());
				CPPUNIT_ASSERT_EQUAL(true, reservation.isRx());
				// All in-between current and next reservation should be IDLE.
				if (i < reserved_time_slots.size() - 1) {
					int next_offset = reserved_time_slots.at(i + 1);
					for (int j = offset + 1; j < next_offset; j++) {
						const Reservation& next_reservation = table_you->getReservation(j);
						CPPUNIT_ASSERT_EQUAL(SYMBOLIC_ID_UNSET, next_reservation.getTarget());
						CPPUNIT_ASSERT_EQUAL(true, next_reservation.isIdle());
					}
				} else {
					for (int j = reserved_time_slots.at(reserved_time_slots.size() - 1) + 1; j < planning_horizon; j++) {
						const Reservation& next_reservation = table_you->getReservation(j);
						CPPUNIT_ASSERT_EQUAL(SYMBOLIC_ID_UNSET, next_reservation.getTarget());
						CPPUNIT_ASSERT_EQUAL(true, next_reservation.isIdle());
					}
				}
			}
//                coutd.setVerbose(false);
		}

		/**
		 * TODO
		 * Link timeout threshold is reached.
		 * Ensures status at both parties are correct, that the sender has sent the request and the receiver has a reply pending.
		 */
		void testLinkIsExpiring() {
			coutd.setVerbose(true);

			// Establish link and send first burst.
			testLinkEstablishment();
			rlc_layer_me->should_there_be_more_data = true;
			LinkManager* lm_me = mac_layer_me->getLinkManager(communication_partner_id);
			unsigned int expected_tx_timeout = lm_me->lme->default_tx_timeout - 1;
			CPPUNIT_ASSERT_EQUAL(expected_tx_timeout, lm_me->lme->tx_timeout);
			// Now increment time until a request is generated.
			uint64_t num_slots_until_request = *std::min_element(lm_me->lme->scheduled_requests.begin(), lm_me->lme->scheduled_requests.end()) - mac_layer_me->getCurrentSlot();
			CPPUNIT_ASSERT(lm_me->lme->tx_timeout > 0);
			CPPUNIT_ASSERT_EQUAL((size_t) lm_me->lme->num_renewal_attempts, lm_me->lme->scheduled_requests.size());
			for (uint64_t i = 0; i < num_slots_until_request; i++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
			}
			// A scheduled request should've been deleted.
			CPPUNIT_ASSERT_EQUAL((size_t) lm_me->lme->num_renewal_attempts - 1, lm_me->lme->scheduled_requests.size());
			// A request should've been sent.
			L2Packet* latest_request = rlc_layer_you->receptions.at(rlc_layer_you->receptions.size() - 1);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, latest_request->getHeaders().at(1)->frame_type);
			// We should now be in the 'awaiting_reply' state.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_me->link_establishment_status);
			// And the next transmission burst should be marked as RX.
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, lm_me->current_reservation_table->getReservation(lm_me->lme->tx_offset).getAction());
			// And the one after that as TX.
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, lm_me->current_reservation_table->getReservation(2 * lm_me->lme->tx_offset).getAction());

			// Increment time until the reply is sent.
			LinkManager* lm_you = mac_layer_you->getLinkManager(own_id);
			CPPUNIT_ASSERT_EQUAL(size_t(1), lm_you->lme->scheduled_replies.size());
			while (!lm_you->lme->scheduled_replies.empty()) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
			}
			CPPUNIT_ASSERT_EQUAL(size_t(0), lm_you->lme->scheduled_replies.size());
			// Renewal should now be complete on our side.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_renewal_complete, lm_me->link_establishment_status);
			// The remaining slots should be marked as TX.
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, lm_me->current_reservation_table->getReservation(lm_me->lme->tx_offset).getAction());

			// Trigger the timeout, which should apply the transition.
			while (lm_me->lme->tx_timeout > 0) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
			}

			// Both sides should have an established link now.
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_me->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_you->link_establishment_status);
			// And agree on the frequency channel.
			CPPUNIT_ASSERT_EQUAL(*lm_me->current_channel, *lm_you->current_channel);

			coutd.setVerbose(false);
		}

		/**
		 * TODO
		 * Link timeout threshold is reached.
		 * Ensures status at both parties are correct after request and reply are exchanged.
		 */
		void testLinkRenewal() {
//				coutd.setVerbose(true);
//				// Do link establishment and send one data packet.
//				testLinkEstablishment();
//				// Now there's more data.
//				rlc_layer->should_there_be_more_data = true;
//				LinkManager* lm_me = mac_layer->getLinkManager(communication_partner_id);
//				// We've sent one message so far, so the link remains valid until default-1.
//				unsigned int expected_tx_timeout = lm_me->lme->default_tx_timeout - 1;
//				CPPUNIT_ASSERT_EQUAL(expected_tx_timeout, lm_me->lme->tx_timeout);
//				// Now increment time until the link should be renewed.
//				CPPUNIT_ASSERT(lm_me->lme->tx_timeout > lm_me->lme->TIMEOUT_THRESHOLD_TRIGGER);
//				size_t num_slots = 0; // Actual number of slots until link renewal is needed.
//				// It should be the difference to reach the THRESHOLD times the transmission offset.
//				// e.g. timeout=5 threshold=1 and we transmit every 3 slots, then 5-1=4 4*3=12 is the expected number of slots
//				size_t expected_num_slots = (lm_me->lme->tx_timeout - lm_me->lme->TIMEOUT_THRESHOLD_TRIGGER) * lm_me->lme->tx_offset;
//				while (lm_me->lme->tx_timeout > lm_me->lme->TIMEOUT_THRESHOLD_TRIGGER) {
//					num_slots++;
//					mac_layer->update(1);
//					mac_layer_you->update(1);
//					std::pair<size_t, size_t> exes_me = mac_layer->execute();
//					std::pair<size_t, size_t> exes_you = mac_layer_you->execute();
//					// The number of transmissions I send must equal the number of receptions you receive...
//					CPPUNIT_ASSERT_EQUAL(exes_me.first, exes_you.second);
//					// ... and vice-versa.
//					CPPUNIT_ASSERT_EQUAL(exes_me.second, exes_you.first);
//					// When I send a transmission, the reservation timeout should decrease.
//					if (exes_me.first == 1)
//						expected_tx_timeout--;
//					CPPUNIT_ASSERT_EQUAL(expected_tx_timeout, lm_me->lme->tx_timeout);
//				}
//				// Ensure our expectation is met.
//				CPPUNIT_ASSERT_EQUAL(expected_num_slots, num_slots);
//				// We should now be in the 'link_about_to_expire' state.
//				CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_about_to_expire, lm_me->link_establishment_status);
//				// So continue until the next transmission slot.
//				while (lm_me->link_establishment_status != LinkManager::awaiting_reply) {
//					mac_layer->update(1);
//					mac_layer_you->update(1);
//					mac_layer->execute();
//					mac_layer_you->execute();
//				}
//				coutd.setVerbose(false);
		}

		/**
		 * TODO
		 * Link timeout threshold is reached.
		 * Ensures that a 2nd request is sent if the first one hasn't arrived.
		 */
		void testLinkExpiringAndLostRequest() {}

		/**
		 * TODO
		 * Link timeout threshold is reached.
		 * Ensures that a 2nd reply is sent if the first one hasn't arrived.
		 */
		void testLinkExpiringAndLostReply() {}

		/**
		 * TODO
		 * Link timeout threshold is reached.
		 * Ensures that if no negotiation has happened prior to expiry, the link is reset to unestablished.
		 */
		void testLinkRenewalFails() {}


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
			CPPUNIT_TEST(testBroadcast);
			CPPUNIT_TEST(testLinkEstablishment);
//            CPPUNIT_TEST(testLinkEstablishmentMultiSlotBurst);
//            CPPUNIT_TEST(testLinkIsExpiring);
//			CPPUNIT_TEST(testLinkRenewal);
//			CPPUNIT_TEST(testEncapsulatedUnicast);
		CPPUNIT_TEST_SUITE_END();
	};

}