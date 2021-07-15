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
	class SystemTests : public CppUnit::TestFixture {
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

			center_frequency1 = env_me->p2p_freq_1;
			center_frequency2 = env_me->p2p_freq_2;
			center_frequency3 = env_me->p2p_freq_3;
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

			phy_layer_me->connected_phys.push_back(phy_layer_you);
			phy_layer_you->connected_phys.push_back(phy_layer_me);

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
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 20;
			CPPUNIT_ASSERT_EQUAL(size_t(0), mac_layer_you->statistic_num_packets_received);
			CPPUNIT_ASSERT_EQUAL(size_t(0), mac_layer_me->statistic_num_packets_received);
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
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_you->statistic_num_requests_received);
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_you->statistic_num_packets_received);
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_me->statistic_num_packets_sent);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(false, lm_me->current_link_state->scheduled_rx_slots.empty());
			for (const auto &pair : lm_me->current_link_state->scheduled_rx_slots) {
				const auto *channel = pair.first;
				unsigned int start_slot = pair.second;
				ReservationTable *table = mac_layer_me->reservation_manager->getReservationTable(channel);
				// RX slots should've been scheduled
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), table->getReservation(start_slot));
				// and all bursts afterwards should've been locked
				for (unsigned int n_burst = 1; n_burst < lm_me->current_link_state->timeout + 1; n_burst++) { // start at 1 since very first burst is reply reception
					for (unsigned int t = 0; t < lm_me->current_link_state->burst_length; t++) {
						int slot = ((int) start_slot) + n_burst*lm_me->burst_offset + t;
						if (t < lm_me->current_link_state->burst_length_tx)
							CPPUNIT_ASSERT_EQUAL(true, table->getReservation(slot).isLocked());
					}
				}
			}


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
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_me->statistic_num_packets_received);
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
					CPPUNIT_ASSERT_EQUAL(true, reservation_rx.isLocked());
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_you->statistic_num_packets_received); // just the request which had been forwarded by the BCLinkManager
			// Jump in time to the next transmission.
			for (size_t t = 0; t < lm_you->burst_offset; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// *Their* status should now show an established link.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
//			// Reservation timeout should be 1 less now.
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout - 1, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(3), rlc_layer_you->receptions.size());
			CPPUNIT_ASSERT_EQUAL(size_t(3), mac_layer_you->statistic_num_packets_received);
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
					CPPUNIT_ASSERT_EQUAL(true, reservation_rx.isLocked());
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_you->receptions.size());
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_you->statistic_num_packets_received);
			// Wait until the next transmission.
//			coutd.setVerbose(true);
			for (size_t t = 0; t < lm_you->burst_offset + lm_you->current_link_state->burst_length; t++) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			// *Their* status should now show an established link.
			CPPUNIT_ASSERT_EQUAL(P2PLinkManager::Status::link_established, mac_layer_you->getLinkManager(own_id)->link_status);
			// Reservation timeout should be 1 less now.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(size_t(3), rlc_layer_you->receptions.size());
			// Ensure reservations match now, with multi-slot TX and matching multi-slot RX.
			for (size_t offset = lm_me->burst_offset - lm_me->current_link_state->burst_length; offset < lm_me->current_link_state->timeout * lm_me->burst_offset; offset += lm_me->burst_offset) {
				const Reservation& reservation_tx = table_me->getReservation(offset);
				const Reservation& reservation_rx = table_you->getReservation(offset);
				coutd << "t=" << offset << " " << reservation_tx << ":" << *table_me->getLinkedChannel() << " " << reservation_rx << ":" << *table_you->getLinkedChannel() << std::endl;
				unsigned int remaining_slots = (unsigned int) expected_num_slots - 1;
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), reservation_tx);
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
//			coutd.setVerbose(true);
			testLinkEstablishment();
			// Don't try to renew the link.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			rlc_layer_you->should_there_be_more_broadcast_data = false;
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
				mac_layer_me->update(1);
				mac_layer_you->update(1);
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

		void testLinkTermination() {
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_you->should_there_be_more_p2p_data = false;

			lm_me->notifyOutgoing(512);
			size_t num_slots = 0, max_slots = 1024;
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

			num_slots = 0;
			while (lm_me->link_status != LinkManager::link_not_established && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			mac_layer_me->update(1);
			mac_layer_you->update(1);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_me->link_status);
			// Everything should be idle.
			for (const auto *table : mac_layer_me->reservation_manager->getP2PReservationTables()) {
				for (int t = 0; t < table->getPlanningHorizon(); t++)
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), table->getReservation(t));
			}
		}

		void testLinkRenewal() {
//			coutd.setVerbose(true);
			testLinkEstablishment();
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			const long packets_so_far = mac_layer_you->statistic_num_packets_received;
			CPPUNIT_ASSERT(packets_so_far > 0);

//			coutd.setVerbose(true);
			size_t num_slots = 0, max_slots = 1024;
//			coutd.setVerbose(true);
			while (lm_me->statistic_num_links_established < 2 && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}

			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(2), lm_me->statistic_num_links_established);
			CPPUNIT_ASSERT( lm_me->link_status != LinkManager::link_not_established);
			CPPUNIT_ASSERT(mac_layer_you->statistic_num_packets_received > packets_so_far);
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
			CPPUNIT_ASSERT_EQUAL(size_t(3), phy_layer_me->outgoing_packets.size());
			L2Packet *packet = phy_layer_me->outgoing_packets.at(2);
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
//			coutd.setVerbose(true);
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
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->current_link_state->timeout, lm_you->current_link_state->timeout);
			for (size_t t = 0; t < lm_me->burst_offset - lm_me->current_link_state->burst_length + 1; t++) {
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
			// Which *shouldn't* have decremented the timeout.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 1, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->current_link_state->timeout, lm_you->current_link_state->timeout);
			// Execute second slot with *you* transmitting.
			mac_layer_me->update(1);
			mac_layer_you->update(1);
			mac_layer_me->execute();
			mac_layer_you->execute();
			mac_layer_me->onSlotEnd();
			mac_layer_you->onSlotEnd();
			// Which *should* have decremented the timeout.
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout - 2, lm_me->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->current_link_state->timeout, lm_you->current_link_state->timeout);
		}

		void testLinkInfoBroadcast() {
//			coutd.setVerbose(true);
			// Single message.
			rlc_layer_me->should_there_be_more_p2p_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 20;
			while (lm_me->link_status != LinkManager::Status::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			// LinkInfo should've been injected.
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer_me->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).size());
			num_slots = 0;
			ReservationTable *bc_table = mac_layer_me->reservation_manager->getBroadcastReservationTable();
			auto *bc_manager = (BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			CPPUNIT_ASSERT(bc_manager->next_broadcast_slot > 0);
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX), bc_table->getReservation(bc_manager->next_broadcast_slot));
			int broadcast_in = (int) bc_manager->next_broadcast_slot;
			for (int t = 0; t < broadcast_in; t++) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}

			while (!rlc_layer_me->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).empty() && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
				if (bc_manager->next_broadcast_scheduled)
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX), bc_table->getReservation(bc_manager->next_broadcast_slot));
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(true, rlc_layer_me->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).empty());
			CPPUNIT_ASSERT_EQUAL(size_t(2), rlc_layer_you->receptions.size());
			num_slots = 0;
			while (lm_you->link_status != LinkManager::Status::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			num_slots = 0;
			while (!rlc_layer_you->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).empty() && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(true, rlc_layer_you->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).empty());
		}

		/**
		 * Tests that if a link request is lost, establishment is re-tried after all reception possibilities have passed.
		 */
		void testReestablishmentAfterDrop() {
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 200;
			CPPUNIT_ASSERT_EQUAL(size_t(0), mac_layer_you->statistic_num_packets_received);

			// Sever connection.
			env_me->phy_layer->connected_phys.clear();

			while (((BCLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->next_broadcast_scheduled && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			// Link request should've been sent, so we're 'awaiting_reply', but as it was dropped, they know nothing of it.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, lm_you->link_status);

			// Reconnect.
			env_me->phy_layer->connected_phys.push_back(env_you->phy_layer);
			num_slots = 0;
			while (lm_you->link_status != LinkManager::Status::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}

			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
		}

		void testSimultaneousRequests() {
//			coutd.setVerbose(true);
			mac_layer_me->notifyOutgoing(512, partner_id);
			mac_layer_you->notifyOutgoing(512, own_id);
			size_t num_slots = 0, max_num_slots = 5000;
			while ((lm_me->link_status != LinkManager::link_established || lm_you->link_status != LinkManager::link_established) && num_slots++ < max_num_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(size_t(1), mac_layer_me->statistic_num_requests_received + mac_layer_you->statistic_num_requests_received);
			CPPUNIT_ASSERT(mac_layer_me->statistic_num_requests_sent + mac_layer_you->statistic_num_requests_sent >= 1); // due to collisions, several attempts may be required
		}

		/**
		 * Tests that burst ends are correctly detected and timeouts changed synchronously.
		 */
		void testTimeout() {
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			// Force bidirectional link.
			lm_me->reported_desired_tx_slots = 1;
			lm_me->notifyOutgoing(512);
			size_t num_slots = 0, max_slots = 1000;

			while (lm_you->link_status != LinkManager::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_established, lm_you->link_status);

			// Link has just been established at RX side, so their reservation must be RX and "ours" TX.
			CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), lm_you->current_reservation_table->getReservation(0));
			CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), lm_me->current_reservation_table->getReservation(0));
			// We have a bidirectional link, so the next reservation should be the other way around.
			CPPUNIT_ASSERT_EQUAL(uint32_t(2), lm_me->current_link_state->burst_length);
			CPPUNIT_ASSERT_EQUAL(uint32_t(2), lm_you->current_link_state->burst_length);
			CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::TX), lm_you->current_reservation_table->getReservation(1));
			CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), lm_me->current_reservation_table->getReservation(1));
			// Neither side should've decremented the timeout.
			CPPUNIT_ASSERT_EQUAL(lm_you->default_timeout, lm_you->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->default_timeout, lm_me->current_link_state->timeout);

			num_slots = 0;
//			coutd.setVerbose(true);
			while (lm_me->link_status != LinkManager::link_not_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
				if (lm_me->current_link_state != nullptr)
					CPPUNIT_ASSERT_EQUAL(lm_you->current_link_state->timeout, lm_me->current_link_state->timeout);
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_you->link_status);

			// Now do reestablishments.
			lm_me->reported_desired_tx_slots = 1;
			lm_me->notifyOutgoing(512);
			rlc_layer_me->should_there_be_more_p2p_data = true;
			size_t num_reestablishments = 10;
//			coutd.setVerbose(true);
			for (size_t n = 0; n < num_reestablishments; n++) {
				num_slots = 0;
				while (lm_you->statistic_num_links_established < (n+1) && num_slots++ < max_slots) {
					try {
						mac_layer_you->update(1);
						mac_layer_me->update(1);
						mac_layer_you->execute();
						mac_layer_me->execute();
						mac_layer_you->onSlotEnd();
						mac_layer_me->onSlotEnd();
					} catch (const std::exception& e) {
						throw std::runtime_error("Error during reestablishment #" + std::to_string(n+1) + ": " + std::string(e.what()));
					}
					CPPUNIT_ASSERT(!(lm_me->link_status == LinkManager::link_established && lm_you->link_status == LinkManager::link_not_established));
					if (lm_me->current_link_state != nullptr && lm_you->current_link_state != nullptr)
						CPPUNIT_ASSERT_EQUAL(lm_you->current_link_state->timeout, lm_me->current_link_state->timeout);
				}
			}
		}

		/**
		 * Tests that two users can re-establish a link many times.
		 */
		void testManyReestablishments() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			lm_me->notifyOutgoing(512);
			size_t num_reestablishments = 10, num_slots = 0, max_slots = 10000;
//			coutd.setVerbose(true);
			while (lm_you->statistic_num_links_established != num_reestablishments && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
				CPPUNIT_ASSERT(!(lm_me->link_status == LinkManager::link_established && lm_you->link_status == LinkManager::link_not_established));
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(num_reestablishments, lm_me->statistic_num_links_established);
			CPPUNIT_ASSERT_EQUAL(num_reestablishments, lm_you->statistic_num_links_established);
		}

	CPPUNIT_TEST_SUITE(SystemTests);
			CPPUNIT_TEST(testLinkEstablishment);
			CPPUNIT_TEST(testLinkEstablishmentMultiSlotBurst);
			CPPUNIT_TEST(testLinkExpiry);
			CPPUNIT_TEST(testLinkExpiryMultiSlot);
			CPPUNIT_TEST(testReservationsUntilExpiry);
			CPPUNIT_TEST(testLinkTermination);
			CPPUNIT_TEST(testLinkRenewal);
			CPPUNIT_TEST(testCommunicateInOtherDirection);
			CPPUNIT_TEST(testCommunicateReverseOrder);
			CPPUNIT_TEST(testPacketSize);
			CPPUNIT_TEST(testReportedTxSlotDesire);
			CPPUNIT_TEST(testLinkInfoBroadcast);
			CPPUNIT_TEST(testReestablishmentAfterDrop);
			CPPUNIT_TEST(testSimultaneousRequests);
			CPPUNIT_TEST(testTimeout);
			CPPUNIT_TEST(testManyReestablishments);

	CPPUNIT_TEST_SUITE_END();
	};
}