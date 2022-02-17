//
// Created by seba on 2/24/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "MockLayers.hpp"
#include "../LinkManager.hpp"
#include "../PPLinkManager.hpp"
#include "../SHLinkManager.hpp"


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
		uint64_t center_frequency1, center_frequency2, center_frequency3, sh_frequency, bandwidth;
		NetworkLayer* net_layer_me, * net_layer_you;
		RLCLayer* rlc_layer_me, * rlc_layer_you;
		ARQLayer* arq_layer_me, * arq_layer_you;
		MACLayer* mac_layer_me, * mac_layer_you;
		PHYLayer* phy_layer_me, * phy_layer_you;
		size_t num_outgoing_bits;

		PPLinkManager *lm_me, *lm_you;
		SHLinkManager *sh_me, *sh_you;

	public:
		void setUp() override {
			own_id = MacId(42);
			partner_id = MacId(43);
			env_me = new TestEnvironment(own_id, partner_id);
			env_you = new TestEnvironment(partner_id, own_id);

			center_frequency1 = env_me->p2p_freq_1;
			center_frequency2 = env_me->p2p_freq_2;
			center_frequency3 = env_me->p2p_freq_3;
			sh_frequency = env_me->sh_frequency;
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
			lm_me = (PPLinkManager*) mac_layer_me->getLinkManager(partner_id);
			lm_you = (PPLinkManager*) mac_layer_you->getLinkManager(own_id);
			sh_me = (SHLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			sh_you = (SHLinkManager*) mac_layer_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
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
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_you->stat_num_packets_rcvd.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_me->stat_num_packets_rcvd.get());
			while (mac_layer_me->stat_num_broadcasts_sent.get() < 1 && num_slots++ < max_slots) {
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
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_you->stat_num_requests_rcvd.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_you->stat_num_packets_rcvd.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_me->stat_num_packets_sent.get());
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->timeout_before_link_expiry, lm_me->link_state.timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->timeout_before_link_expiry, lm_you->link_state.timeout);						


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
			// Link reply + first data tx should've arrived, so *our* link should be established...
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_me->getLinkManager(partner_id)->link_status);
			CPPUNIT_ASSERT_EQUAL(size_t(2), (size_t) mac_layer_me->stat_num_packets_rcvd.get());
			// ... and *their* link should also be established
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_you->getLinkManager(own_id)->link_status);
			// Reservation timeouts should have been decremented once
			CPPUNIT_ASSERT_EQUAL(lm_me->timeout_before_link_expiry - 1, lm_me->link_state.timeout);
			CPPUNIT_ASSERT_EQUAL(lm_you->timeout_before_link_expiry - 1, lm_you->link_state.timeout);
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
			unsigned int req_tx_slots = 3, expected_num_slots = req_tx_slots + 1;
			lm_me->outgoing_traffic_estimate.put(req_tx_slots * bits_per_slot);
			unsigned int required_slots = lm_me->getRequiredTxSlots() + lm_me->getRequiredRxSlots();
			CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(req_tx_slots * bits_per_slot, partner_id);
			while (mac_layer_me->stat_num_broadcasts_sent.get() < 1) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);				
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				lm_me->outgoing_traffic_estimate.put(req_tx_slots * bits_per_slot);
				required_slots = lm_me->getRequiredTxSlots() + lm_me->getRequiredRxSlots();
				CPPUNIT_ASSERT_EQUAL(expected_num_slots, required_slots);
			}
			// Ensure that the request requested a multi-slot reservation.
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer_me->outgoing_packets.size());
			L2Packet* request = phy_layer_me->outgoing_packets.at(0);
			CPPUNIT_ASSERT(request->getRequestIndex() > -1);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, request->getHeaders().at(request->getRequestIndex())->frame_type);
			required_slots = lm_me->getRequiredTxSlots() + lm_me->getRequiredRxSlots();
			CPPUNIT_ASSERT(required_slots > 1);
			// Link request should've been sent, so we're 'awaiting_reply'.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, lm_me->link_status);
			// Reservation timeout should still be default.
			CPPUNIT_ASSERT_EQUAL(lm_me->timeout_before_link_expiry, lm_me->link_state.timeout);
			// Increment time until status is 'link_established'.
			size_t num_slots = 0, max_slots = 200;
			while (!(lm_me->link_status == LinkManager::link_established && lm_you->link_status == LinkManager::link_established) && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			// Link reply should've arrived, so both links should be established...
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_me->getLinkManager(partner_id)->link_status);			
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_you->getLinkManager(own_id)->link_status);			;

			// Make sure that all corresponding slots are marked as TX on our side,
			ReservationTable* table_me = lm_me->current_reservation_table;
			ReservationTable* table_you = lm_you->current_reservation_table;			
			auto tx_rx_slots = lm_me->getReservations();						
			for (auto tx_slot : tx_rx_slots.first) {
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), table_me->getReservation(tx_slot));
				CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), table_you->getReservation(tx_slot));
			}
			for (auto rx_slot : tx_rx_slots.second) {
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), table_me->getReservation(rx_slot));
				CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::TX), table_you->getReservation(rx_slot));
			}			
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
			unsigned int expected_tx_timeout = lm_me->timeout_before_link_expiry - 1;			
			CPPUNIT_ASSERT_EQUAL(expected_tx_timeout, lm_me->link_state.timeout);

			// Now increment time until the link expires.
			size_t num_slots = 0, max_num_slots = lm_me->timeout_before_link_expiry * lm_me->default_burst_offset + lm_me->default_burst_offset;
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
			unsigned int required_tx_slots = 3;
			lm_me->outgoing_traffic_estimate.put(required_tx_slots * bits_per_slot);
			unsigned int expected_num_slots = required_tx_slots + 1;
			unsigned int required_slots = lm_me->getRequiredTxSlots() + lm_me->getRequiredRxSlots();
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
			CPPUNIT_ASSERT_EQUAL(lm_me->timeout_before_link_expiry - 1, lm_me->link_state.timeout);
			CPPUNIT_ASSERT_EQUAL(lm_me->timeout_before_link_expiry - 1, lm_you->link_state.timeout);			

			num_slots = 0;
			while (lm_me->link_status != LinkManager::link_not_established && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
				if (lm_me->link_status != LinkManager::link_not_established) {					
					for (int t = 1; t < planning_horizon; t++) {
						const Reservation& res_tx = lm_me->current_reservation_table->getReservation(t);
						const Reservation& res_rx = lm_you->current_reservation_table->getReservation(t);
						if (res_tx.isTx()) {							
							CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), res_tx);
							CPPUNIT_ASSERT_EQUAL(Reservation(own_id, Reservation::RX), res_rx);
						}
					}					
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
			const long packets_so_far = (size_t) mac_layer_you->stat_num_packets_rcvd.get();
			CPPUNIT_ASSERT(packets_so_far > 0);

//			coutd.setVerbose(true);
			size_t num_slots = 0, max_slots = 1024;
//			coutd.setVerbose(true);
			while (int(mac_layer_me->stat_num_pp_links_established.get()) < 2 && num_slots++ < max_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}

			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(2), size_t(mac_layer_me->stat_num_pp_links_established.get()));
			CPPUNIT_ASSERT( lm_me->link_status != LinkManager::link_not_established);
			CPPUNIT_ASSERT(((size_t) mac_layer_you->stat_num_packets_rcvd.get()) > packets_so_far);
		}

		/** Before introducing the onSlotEnd() function, success depended on the order of the execute() calls (which is of course terrible),
		 * so this test ensures that the order in which user actions are executed doesn't matter.
		 */
		void testCommunicateInOtherDirection() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;			
			// Other guy tries to communicate with us.
			mac_layer_you->notifyOutgoing(512, own_id);
			while (lm_you->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				mac_layer_me->update(1);
				mac_layer_you->update(1);
				mac_layer_me->execute();
				mac_layer_you->execute();
				mac_layer_me->onSlotEnd();
				mac_layer_you->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
		}

		/** Before introducing the onSlotEnd() function, success depended on the order of the execute() calls (which is of course terrible),
		 * so this test ensures that the order in which user actions are executed doesn't matter.
		 */
		void testCommunicateReverseOrder() {
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// Do link establishment.
			size_t num_slots = 0, max_num_slots = 100;			
			mac_layer_me->notifyOutgoing(512, partner_id);
			while (lm_me->link_status != LinkManager::Status::link_established && num_slots++ < max_num_slots) {
				// you first, then me
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_you->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
		}

		void testPacketSize() {
			testLinkEstablishment();
			CPPUNIT_ASSERT_EQUAL( phy_layer_me->outgoing_packets.empty(), false);
			for (L2Packet *packet : phy_layer_me->outgoing_packets)
				CPPUNIT_ASSERT(phy_layer_me->getCurrentDatarate() >= packet->getBits());
		}				

		/**
		 * Tests that if a link request is lost, establishment is re-tried after all reception possibilities have passed.
		 */
		void testReestablishmentAfterDrop() {
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 200;
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_you->stat_num_packets_rcvd.get());

			// Sever connection.
			env_me->phy_layer->connected_phys.clear();

			while (mac_layer_me->stat_num_broadcasts_sent.get() < 1 && num_slots++ < max_slots) {
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
			while (lm_me->link_status != LinkManager::Status::link_established && num_slots++ < max_slots) {
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
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_me->stat_num_requests_rcvd.get() + (size_t) mac_layer_you->stat_num_requests_rcvd.get());
			CPPUNIT_ASSERT((size_t) mac_layer_me->stat_num_requests_sent.get() + (size_t) mac_layer_you->stat_num_requests_sent.get() >= 1); // due to collisions, several attempts may be required
		}

		/**
		 * Tests that burst ends are correctly detected and timeouts changed synchronously.
		 */
		void testTimeout() {
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			// Force bidirectional link.
			lm_me->setForceBidirectionalLinks(true);
			lm_me->notifyOutgoing(512);
			size_t num_slots = 0, max_slots = 1000;

			while (lm_me->link_status != LinkManager::link_established && num_slots++ < max_slots) {
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

			num_slots = 0;
//			coutd.setVerbose(true);
			while (lm_me->link_status != LinkManager::link_not_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
				if (lm_me->link_status != LinkManager::link_not_established)
					CPPUNIT_ASSERT_EQUAL(lm_you->link_state.timeout, lm_me->link_state.timeout);
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::link_not_established, lm_you->link_status);

			// Now do reestablishments.
			lm_me->setForceBidirectionalLinks(true);
			lm_me->notifyOutgoing(512);
			rlc_layer_me->should_there_be_more_p2p_data = true;
			size_t num_reestablishments = 10;
//			coutd.setVerbose(true);
			for (size_t n = 0; n < num_reestablishments; n++) {
				num_slots = 0;
				while (int(mac_layer_you->stat_num_pp_links_established.get()) < (n+1) && num_slots++ < max_slots) {
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
					if (lm_me->link_status != LinkManager::link_not_established && lm_you->link_status != LinkManager::link_not_established)
						CPPUNIT_ASSERT_EQUAL(lm_you->link_state.timeout, lm_me->link_state.timeout);
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
			while (((int) mac_layer_me->stat_num_pp_links_established.get()) != num_reestablishments && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
				CPPUNIT_ASSERT(!(lm_me->link_status == LinkManager::link_established && lm_you->link_status == LinkManager::link_not_established));
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(num_reestablishments, size_t(mac_layer_me->stat_num_pp_links_established.get()));
			CPPUNIT_ASSERT_EQUAL(num_reestablishments, size_t(mac_layer_me->stat_num_pp_links_established.get()));
		}

		/** Ensure that the communication partner correctly sets slot reservations based on the advertised next broadcast slot. */
		void testSlotAdvertisement() {
			mac_layer_me->setAlwaysScheduleNextBroadcastSlot(true);
			rlc_layer_me->should_there_be_more_broadcast_data = true;
			auto *bc_link_manager_me = (SHLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			auto *bc_link_manager_you = (SHLinkManager*) mac_layer_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			bc_link_manager_me->notifyOutgoing(1);
			// proceed until the first broadcast's been received
			while (rlc_layer_you->receptions.empty()) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			// next broadcast should've been scheduled
			CPPUNIT_ASSERT_EQUAL(true, bc_link_manager_me->next_broadcast_scheduled);
			// and reservations should match between both communication partners
			CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX), bc_link_manager_me->current_reservation_table->getReservation(bc_link_manager_me->next_broadcast_slot));
			CPPUNIT_ASSERT_EQUAL(Reservation(mac_layer_me->getMacId(), Reservation::RX), bc_link_manager_you->current_reservation_table->getReservation(bc_link_manager_me->next_broadcast_slot));
			size_t max_t = 1000;
			for (size_t t = 0; t < max_t; t++) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
				// next broadcast slot should've been scheduled
				CPPUNIT_ASSERT_EQUAL(true, bc_link_manager_me->next_broadcast_scheduled);
				// and reservations should match between both communication partners
				CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX), bc_link_manager_me->current_reservation_table->getReservation(bc_link_manager_me->next_broadcast_slot));
				CPPUNIT_ASSERT_EQUAL(Reservation(mac_layer_me->getMacId(), Reservation::RX), bc_link_manager_you->current_reservation_table->getReservation(bc_link_manager_me->next_broadcast_slot));
			}
		}

		/** When a link reply is sent, the sender should schedule *all* reservations of the P2P link s.t. a possibly-lost first packet doesn't prevent the link from being properly set up. */
		void testScheduleAllReservationsWhenLinkReplyIsSent() {			
			auto *p2p_lm_me = mac_layer_me->getLinkManager(partner_id), *p2p_lm_you = mac_layer_you->getLinkManager(own_id);			
			// have comm. partner establish a link
			p2p_lm_you->notifyOutgoing(512);
			size_t max_t = 100, t = 0;
			// wait until *we* have transmitted a reply
			for (;t < max_t && ((size_t) mac_layer_me->stat_num_replies_sent.get()) < 1; t++) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_t, t);
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_me->stat_num_replies_sent.get());
			// now make sure that for the entire P2P link, all reservations are scheduled
			const auto *table_me = p2p_lm_me->current_reservation_table, *table_you = p2p_lm_you->current_reservation_table;
			CPPUNIT_ASSERT_EQUAL(table_me->getLinkedChannel()->getCenterFrequency(), table_you->getLinkedChannel()->getCenterFrequency());			
			for (size_t i = 0; i < table_me->getPlanningHorizon(); i++) {
				const auto res_me = table_me->getReservation(i), res_you = table_you->getReservation(i);
				if (res_you.isAnyTx() || res_you.isAnyRx()) {
					// std::cout << "t=" << i << ": " << res_me << " vs " << res_you << std::endl;
					// any communication resource that *you* have scheduled should likewise be scheduled for me
					CPPUNIT_ASSERT_EQUAL(true, res_me.isTx() || res_me.isRx());
				}
			}
		}

		/** My link is established after I've sent my link reply and receive the first data packet. If that doesn't arrive within as many attempts as ARQ allows, I should close the link. */
		void testGiveUpLinkIfFirstDataPacketDoesntComeThrough() {			
			auto *p2p_lm_me = mac_layer_me->getLinkManager(partner_id), *p2p_lm_you = mac_layer_you->getLinkManager(own_id);			
			// have comm. partner establish a link
			p2p_lm_you->notifyOutgoing(512);
			env_me->rlc_layer->should_there_be_more_p2p_data = false;
			env_you->rlc_layer->should_there_be_more_p2p_data = false;
			size_t max_t = 1000, t = 0;
			// wait until *we* have transmitted a reply
			for (;t < max_t && ((size_t) mac_layer_me->stat_num_replies_sent.get()) < 1; t++) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_t, t);
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_me->stat_num_replies_sent.get());
			// drop all packets from now on
			env_me->phy_layer->connected_phys.clear();
			env_you->phy_layer->connected_phys.clear();
			t = 0;
			for (;t < max_t && p2p_lm_me->link_status != LinkManager::Status::link_not_established; t++) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_t, t);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, p2p_lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_me->stat_num_links_closed_early.get());
		}

		void testMACDelays() {
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			rlc_layer_me->num_remaining_broadcast_packets = 4;
			auto *bc_link_manager_me = (SHLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			auto *bc_link_manager_you = (SHLinkManager*) mac_layer_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			bc_link_manager_me->notifyOutgoing(1);
			// proceed until the first broadcast's been received
			size_t num_broadcasts = 1;
			size_t num_slots = 0, max_slots = 100;
			while (rlc_layer_you->receptions.size() < num_broadcasts && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();				
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			// check statistics
			CPPUNIT_ASSERT_EQUAL(num_broadcasts, (size_t) mac_layer_me->stat_num_broadcasts_sent.get());
			CPPUNIT_ASSERT_EQUAL(num_broadcasts, (size_t) mac_layer_you->stat_num_broadcasts_rcvd.get());			
			CPPUNIT_ASSERT_EQUAL(mac_layer_me->stat_broadcast_selected_candidate_slots.get(), mac_layer_me->stat_broadcast_mac_delay.get());
			// proceed further
			num_broadcasts = 3;			
			num_slots = 0; 
			max_slots = 1000;			
			while (rlc_layer_you->receptions.size() < num_broadcasts && num_slots++ < max_slots) {
				bc_link_manager_me->notifyOutgoing(1);
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();				
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			// check statistics
			CPPUNIT_ASSERT_EQUAL(num_broadcasts, (size_t) mac_layer_me->stat_num_broadcasts_sent.get());
			CPPUNIT_ASSERT_EQUAL(num_broadcasts, (size_t) mac_layer_you->stat_num_broadcasts_rcvd.get());			
			CPPUNIT_ASSERT_EQUAL(mac_layer_me->stat_broadcast_selected_candidate_slots.get(), mac_layer_me->stat_broadcast_mac_delay.get());
		}

		void testCompareBroadcastSlotSetSizesToAnalyticalExpectations_TargetCollisionProbs() {
			rlc_layer_you->should_there_be_more_broadcast_data = true;
			auto *bc_link_manager_me = (SHLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			bc_link_manager_me->setUseContentionMethod(ContentionMethod::binomial_estimate);
			auto *bc_link_manager_you = (SHLinkManager*) mac_layer_you->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);			
			// "you" broadcast
			bc_link_manager_you->notifyOutgoing(1);
			// "I" select slots
			// first of, without any neighbors
			std::vector<double> target_collision_probs = {0.01, 0.05, 0.15, 0.25};
			for (auto p : target_collision_probs) {
				unsigned int num_candidate_slots = bc_link_manager_me->getNumCandidateSlots(p, bc_link_manager_me->MIN_CANDIDATES, bc_link_manager_me->MAX_CANDIDATES);
				CPPUNIT_ASSERT_EQUAL(bc_link_manager_me->MIN_CANDIDATES, num_candidate_slots);
			}			
			// now add a neighbor
			size_t num_slots = 0, max_slots = 1000;
			while (bc_link_manager_me->contention_estimator.getNumActiveNeighbors() < 1) {
				bc_link_manager_me->notifyOutgoing(1);
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();				
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);			
			CPPUNIT_ASSERT_EQUAL(uint(1), bc_link_manager_me->contention_estimator.getNumActiveNeighbors());			
			for (auto p : target_collision_probs) {
				unsigned int num_candidate_slots = bc_link_manager_me->getNumCandidateSlots(p, bc_link_manager_me->MIN_CANDIDATES, bc_link_manager_me->MAX_CANDIDATES);
				CPPUNIT_ASSERT_GREATER(bc_link_manager_me->MIN_CANDIDATES, num_candidate_slots);
				double num_neighbors = 1.0;
				unsigned int expected_candidate_slots = (unsigned int) std::ceil(1.0 / (1.0 - pow(1.0 - p, (1/num_neighbors))));												
				CPPUNIT_ASSERT_EQUAL(expected_candidate_slots, num_candidate_slots);
			}			
		}

		void testLinkRequestIsCancelledWhenAnotherIsReceived() {
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			// both want to establish a link exactly at the same time			
			mac_layer_me->notifyOutgoing(512, partner_id);
			mac_layer_you->notifyOutgoing(512, own_id);			
			size_t num_slots = 0, max_slots = 1000;			
			while ((((size_t) mac_layer_me->stat_num_requests_rcvd.get()) < 1 && ((size_t) mac_layer_you->stat_num_requests_rcvd.get()) < 1) && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
				mac_layer_me->notifyOutgoing(512, partner_id);
				mac_layer_you->notifyOutgoing(512, own_id);
			}			
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);			
			CPPUNIT_ASSERT(((size_t) mac_layer_me->stat_num_requests_rcvd.get()) < 1 || ((size_t) mac_layer_you->stat_num_requests_rcvd.get()) < 1);		
			// there should be exactly *one* link request that made it through
			if ((size_t) mac_layer_me->stat_num_requests_rcvd.get() == 1) 
				CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_you->stat_num_requests_rcvd.get());				
			else 
				CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_me->stat_num_requests_rcvd.get());							
		}

		void testForcedBidirectionalLinks() {
			mac_layer_me->setForceBidirectionalLinks(true);
			mac_layer_you->setForceBidirectionalLinks(true);
			CPPUNIT_ASSERT_EQUAL(uint(1), lm_me->reported_resoure_requirement);
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			// New data for communication partner.
			mac_layer_me->notifyOutgoing(512, partner_id);
			size_t num_slots = 0, max_slots = 100;			
			while (lm_me->link_status != LinkManager::Status::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
			size_t num_tx_reservations = 0, num_rx_reservations = 0;
			for (size_t t = 0; t < lm_me->default_burst_offset; t++) {
				const auto& res = lm_me->current_reservation_table->getReservation(t);				
				if (res.isAnyTx())
					num_tx_reservations++;
				else if (res.isAnyRx())
					num_rx_reservations++;
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), num_tx_reservations);
			CPPUNIT_ASSERT_EQUAL(size_t(1), num_rx_reservations);
		}

		void testNoEmptyBroadcasts() {
			// always schedule new slots
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			rlc_layer_me->num_remaining_broadcast_packets = 1;
			mac_layer_me->setAlwaysScheduleNextBroadcastSlot(true);
			mac_layer_me->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
			size_t num_slots = 0, max_num_slots = 100;
			// broadcast once
			while (((size_t) mac_layer_me->stat_num_broadcasts_sent.get()) < size_t(1) && num_slots++ < max_num_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_num_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_me->stat_num_broadcasts_sent.get());
			// no more data	
			CPPUNIT_ASSERT_EQUAL(size_t(0), rlc_layer_me->num_remaining_broadcast_packets);		
			SHLinkManager *bc_lm = (SHLinkManager*) mac_layer_me->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			// but there should be another TX reservation
			size_t num_tx_reservations = 0;
			for (size_t t = 0; t < planning_horizon; t++) {
				if (bc_lm->current_reservation_table->getReservation(t).isAnyTx())
					num_tx_reservations++;
			}
			CPPUNIT_ASSERT_GREATER(size_t(0), num_tx_reservations);
			mac_layer_me->setAlwaysScheduleNextBroadcastSlot(false);
			num_slots = 0;
			while (bc_lm->next_broadcast_scheduled && num_slots++ < max_num_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_num_slots, num_slots);
			CPPUNIT_ASSERT(!bc_lm->next_broadcast_scheduled);
			// no more packets should've been sent
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_me->stat_num_broadcasts_sent.get());
		}
		
		/**
		 * When there's no broadcasts going on, link requests should be base header + broadcast header + link request.
		 * */
		void testLinkRequestPacketsNoBroadcasts() {			
			size_t num_slots = 0, max_slots = 1000;
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			lm_me->notifyOutgoing(512);			
			while (((int) mac_layer_me->stat_num_pp_links_established.get()) < 2 && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(2), size_t(mac_layer_me->stat_num_pp_links_established.get()));						
			for (auto *packet : phy_layer_me->outgoing_packets) {
				if (packet->getRequestIndex() != -1) {					
					CPPUNIT_ASSERT_EQUAL(size_t(3), packet->getHeaders().size());
					CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, packet->getHeaders().at(0)->frame_type);
					CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::broadcast, packet->getHeaders().at(1)->frame_type);
					CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, packet->getHeaders().at(2)->frame_type);
				}
			}			
		}

		/**
		 * When there's broadcasts going on, link requests should be base header + broadcast + link request.
		 * */
		void testLinkRequestPacketsWithBroadcasts() {						
			size_t num_slots = 0, max_slots = 1000;
			rlc_layer_me->should_there_be_more_p2p_data = true;
			rlc_layer_me->should_there_be_more_broadcast_data = true;
			rlc_layer_you->should_there_be_more_broadcast_data = false;
			lm_me->notifyOutgoing(512);
			mac_layer_you->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
			while (((int) mac_layer_me->stat_num_pp_links_established.get()) < 2 && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(2), size_t(mac_layer_me->stat_num_pp_links_established.get()));						
			for (auto *packet : phy_layer_me->outgoing_packets) {
				if (packet->getRequestIndex() != -1) {					
					CPPUNIT_ASSERT_EQUAL(size_t(3), packet->getHeaders().size());
					CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, packet->getHeaders().at(0)->frame_type);
					CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::broadcast, packet->getHeaders().at(1)->frame_type);
					CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, packet->getHeaders().at(2)->frame_type);
				}
			}			
		}

		/**
		 * From issue 102: https://collaborating.tuhh.de/e-4/research-projects/intairnet-collection/mc-sotdma/-/issues/102
		 * */
		void testMissedLastLinkEstablishmentOpportunity() {
			// don't attempt to re-establish
			rlc_layer_me->should_there_be_more_p2p_data = false;
			rlc_layer_you->should_there_be_more_p2p_data = false;
			lm_me->notifyOutgoing(512);
			size_t num_slots = 0, max_slots = 100;
			while (mac_layer_me->stat_num_requests_sent.get() < 1 && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_me->stat_num_requests_sent.get());
			CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_reply, lm_me->link_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::awaiting_data_tx, lm_you->link_status);
			// packet drops from now on
			phy_layer_me->connected_phys.clear();
			phy_layer_you->connected_phys.clear();
			num_slots = 0;
			for (size_t t = 0; t < max_slots; t++) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			// both sides should've entered link establishment by now
			CPPUNIT_ASSERT(lm_me->link_status == LinkManager::awaiting_request_generation || lm_me->link_status == LinkManager::awaiting_reply);
			CPPUNIT_ASSERT(lm_you->link_status == LinkManager::awaiting_request_generation || lm_you->link_status == LinkManager::awaiting_reply);
			// and should have sent a couple of requests
			CPPUNIT_ASSERT_GREATER(size_t(1), (size_t) mac_layer_me->stat_num_requests_sent.get());
			CPPUNIT_ASSERT_GREATER(size_t(1), (size_t) mac_layer_you->stat_num_requests_sent.get());
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_me->stat_num_requests_rcvd.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_you->stat_num_requests_rcvd.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_you->stat_pp_link_missed_first_data_tx.get());
		}

		void testMissedAndReceivedPacketsMatch() {
			// random access
			mac_layer_me->setContentionMethod(ContentionMethod::naive_random_access);
			mac_layer_you->setContentionMethod(ContentionMethod::naive_random_access);
			// no slot advertising
			mac_layer_me->setAlwaysScheduleNextBroadcastSlot(false);
			mac_layer_you->setAlwaysScheduleNextBroadcastSlot(false);
			rlc_layer_me->should_there_be_more_broadcast_data = false;
			rlc_layer_you->should_there_be_more_broadcast_data = false;
			// make sure actual data is sent
			rlc_layer_me->always_return_broadcast_payload = true;
			rlc_layer_you->always_return_broadcast_payload = true;
			// select randomly from three idle slots
			mac_layer_me->setBcSlotSelectionMinNumCandidateSlots(3);
			mac_layer_you->setBcSlotSelectionMinNumCandidateSlots(3);
			mac_layer_me->setBcSlotSelectionMaxNumCandidateSlots(3);
			mac_layer_you->setBcSlotSelectionMaxNumCandidateSlots(3);									
			// both have stuff to send
			mac_layer_me->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
			mac_layer_you->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
			size_t max_slots = 100;
			for (size_t t = 0; t < max_slots; t++) {
				mac_layer_me->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				mac_layer_you->notifyOutgoing(512, SYMBOLIC_LINK_ID_BROADCAST);
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();				
			}			
			CPPUNIT_ASSERT_GREATER(size_t(0), (size_t) mac_layer_me->stat_num_broadcasts_sent.get());
			CPPUNIT_ASSERT_GREATER(size_t(0), (size_t) mac_layer_you->stat_num_broadcasts_sent.get());
			CPPUNIT_ASSERT_GREATER(size_t(0), (size_t) mac_layer_me->stat_num_broadcasts_rcvd.get());
			CPPUNIT_ASSERT_GREATER(size_t(0), (size_t) mac_layer_you->stat_num_broadcasts_rcvd.get());
			// there should've been collisions
			CPPUNIT_ASSERT_LESS((size_t) mac_layer_me->stat_num_broadcasts_sent.get(), (size_t) mac_layer_you->stat_num_broadcasts_rcvd.get());
			CPPUNIT_ASSERT_LESS((size_t) mac_layer_you->stat_num_broadcasts_sent.get(), (size_t) mac_layer_me->stat_num_broadcasts_rcvd.get());			
			// which should be identical to sent-received
			size_t num_broadcasts_sent_me = (size_t) mac_layer_me->stat_num_broadcasts_sent.get();
			size_t num_beacons_sent_me = (size_t) mac_layer_me->stat_num_beacons_sent.get();
			size_t num_broadcasts_rcvd_me = (size_t) mac_layer_me->stat_num_broadcasts_rcvd.get();
			size_t num_beacons_rcvd_me = (size_t) mac_layer_me->stat_num_beacons_rcvd.get();
			

			size_t num_broadcasts_sent_you = (size_t) mac_layer_you->stat_num_broadcasts_sent.get();
			size_t num_beacons_sent_you = (size_t) mac_layer_you->stat_num_beacons_sent.get();
			size_t num_broadcasts_rcvd_you = (size_t) mac_layer_you->stat_num_broadcasts_rcvd.get();
			size_t num_beacons_rcvd_you = (size_t) mac_layer_you->stat_num_beacons_rcvd.get();
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_you->stat_num_packet_collisions.get());
			CPPUNIT_ASSERT_EQUAL((size_t) phy_layer_you->stat_num_packets_missed.get(), num_broadcasts_sent_me + num_beacons_sent_me - num_broadcasts_rcvd_you - num_beacons_rcvd_you);			
			CPPUNIT_ASSERT_EQUAL((size_t) phy_layer_me->stat_num_packets_missed.get(), num_broadcasts_sent_you + num_beacons_sent_you - num_broadcasts_rcvd_me - num_beacons_rcvd_me);			
		}

		void testPPLinkEstablishmentTime() {
			mac_layer_me->notifyOutgoing(512, partner_id);
			CPPUNIT_ASSERT_EQUAL(size_t(0), (size_t) mac_layer_me->stat_num_pp_links_established.get());
			size_t num_slots = 0, max_slots = 512;
			while (lm_me->link_status != LinkManager::Status::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, lm_me->link_status);
			CPPUNIT_ASSERT_GREATER(0.0, mac_layer_me->stat_pp_link_establishment_time.get());
			CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_layer_me->stat_num_pp_links_established.get());			
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
//			CPPUNIT_TEST(testPacketSize);						
			CPPUNIT_TEST(testReestablishmentAfterDrop);
			CPPUNIT_TEST(testSimultaneousRequests);
			CPPUNIT_TEST(testTimeout);
			CPPUNIT_TEST(testManyReestablishments);
			CPPUNIT_TEST(testSlotAdvertisement);
			CPPUNIT_TEST(testScheduleAllReservationsWhenLinkReplyIsSent);
			// CPPUNIT_TEST(testGiveUpLinkIfFirstDataPacketDoesntComeThrough);
			CPPUNIT_TEST(testMACDelays);			
			CPPUNIT_TEST(testCompareBroadcastSlotSetSizesToAnalyticalExpectations_TargetCollisionProbs);			
			CPPUNIT_TEST(testLinkRequestIsCancelledWhenAnotherIsReceived);		
			CPPUNIT_TEST(testForcedBidirectionalLinks);					
			CPPUNIT_TEST(testNoEmptyBroadcasts);			
			CPPUNIT_TEST(testLinkRequestPacketsNoBroadcasts);			
			CPPUNIT_TEST(testLinkRequestPacketsWithBroadcasts);		
			CPPUNIT_TEST(testMissedLastLinkEstablishmentOpportunity);					
			CPPUNIT_TEST(testMissedAndReceivedPacketsMatch);
			CPPUNIT_TEST(testPPLinkEstablishmentTime);
	CPPUNIT_TEST_SUITE_END();
	};
}