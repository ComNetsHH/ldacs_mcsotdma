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

		MacId own_id, communication_partner_id;
		uint32_t planning_horizon;
		uint64_t center_frequency1, center_frequency2, center_frequency3, bc_frequency, bandwidth;
		NetworkLayer* net_layer_me, * net_layer_you;
		RLCLayer* rlc_layer_me, * rlc_layer_you;
		ARQLayer* arq_layer_me, * arq_layer_you;
		MACLayer* mac_layer_me, * mac_layer_you;
		PHYLayer* phy_layer_me, * phy_layer_you;
		size_t num_outgoing_bits;

	public:
		void setUp() override {
			own_id = MacId(42);
			communication_partner_id = MacId(43);
			env_me = new TestEnvironment(own_id, communication_partner_id, true);
			env_you = new TestEnvironment(communication_partner_id, own_id, true);

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
			mac_layer_me->notifyOutgoing(512, communication_partner_id);
			size_t num_slots = 0, max_slots = 20;
			auto* lm_me = (P2PLinkManager*) mac_layer_me->getLinkManager(communication_partner_id);
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
			while (((LinkManager*) mac_layer_me->getLinkManager(communication_partner_id))->link_status != LinkManager::link_established && num_slots++ < max_slots) {
				mac_layer_you->update(1);
				mac_layer_me->update(1);
				mac_layer_you->execute();
				mac_layer_me->execute();
				mac_layer_you->onSlotEnd();
				mac_layer_me->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_slots);
			// Link reply should've arrived, so *our* link should be established...
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, mac_layer_me->getLinkManager(communication_partner_id)->link_status);
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
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation_tx.getTarget());
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
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation_tx.getTarget());
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
			auto *lm_me = (P2PLinkManager*) mac_layer_me->getLinkManager(communication_partner_id);
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


	CPPUNIT_TEST_SUITE(NewSystemTests);
			CPPUNIT_TEST(testLinkEstablishment);
			CPPUNIT_TEST(testLinkExpiry);
		CPPUNIT_TEST_SUITE_END();
	};
}