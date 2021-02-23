//
// Created by seba on 2/18/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../P2PLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class P2PLinkManagerTests : public CppUnit::TestFixture {
	private:
		uint32_t planning_horizon = 1024;
		P2PLinkManager *link_manager;
		MacId own_id, partner_id;
		TestEnvironment *env;
		ReservationManager *reservation_manager;

	public:
		void setUp() override {
			own_id = MacId(42);
			partner_id = MacId(43);
			env = new TestEnvironment(own_id, partner_id, true);
			link_manager = (P2PLinkManager*) env->mac_layer->getLinkManager(partner_id);
			reservation_manager = env->mac_layer->getReservationManager();
		}

		void tearDown() override {
			delete env;
		}

		void testP2PSlotSelectionHelper(bool is_init) {
			//			coutd.setVerbose(true);
			unsigned int num_channels = 1, num_slots = 3, min_offset = 2, burst_length = 5, burst_length_tx = 3;
			auto map = link_manager->p2pSlotSelection(num_channels, num_slots, min_offset, burst_length, burst_length_tx, is_init);
			CPPUNIT_ASSERT_EQUAL(size_t(num_channels), map.size());
			std::vector<unsigned int> expected_slots = {2, 3, 4, 5, 6, 7, 8},
					expected_slots_tx = {2, 3, 4, 5, 6},
					expected_slots_rx = {5, 6, 7, 8};
			const FrequencyChannel* channel = map.begin()->first;
			const std::vector<unsigned int> start_offsets = map.begin()->second;

			// All slots should be reserved locally.
			for (unsigned int offset : expected_slots)
				CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED), reservation_manager->getReservationTable(channel)->getReservation(offset));
			// For the first couple of slots the transmitter should be reserved.
			for (unsigned int offset : expected_slots_tx)
				CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED), reservation_manager->getTxTable()->getReservation(offset));
			// For the latter slots a receiver should be reserved.
			for (unsigned int offset : expected_slots_rx) {
				CPPUNIT_ASSERT_EQUAL(true, std::any_of(reservation_manager->getRxTables().begin(), reservation_manager->getRxTables().end(), [offset](ReservationTable *table) {
					return table->getReservation(offset) == Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED);
				}));
			}
			// Since this is an initial slot selection, for the burst start slots a receiver should also be reserved.
			bool expect_rx_to_be_reserved = is_init;
			for (unsigned int offset : start_offsets)
				CPPUNIT_ASSERT_EQUAL(expect_rx_to_be_reserved, std::any_of(reservation_manager->getRxTables().begin(), reservation_manager->getRxTables().end(), [offset](ReservationTable *table) {
					return table->getReservation(offset) == Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED);
				}));

//			coutd.setVerbose(false);
		}

		void testInitialP2PSlotSelection() {
			testP2PSlotSelectionHelper(true);
		}

		void testRenewalP2PSlotSelection() {
			testP2PSlotSelectionHelper(false);
		}

		void testMultiChannelP2PSlotSelection() {
//			coutd.setVerbose(true);
			unsigned int num_channels = 3, num_slots = 3, min_offset = 2, burst_length = 5, burst_length_tx = 3;
			auto map = link_manager->p2pSlotSelection(num_channels, num_slots, min_offset, burst_length, burst_length_tx, false);
			// As many entries as channels.
			CPPUNIT_ASSERT_EQUAL(size_t(num_channels), map.size());
			for (auto item : map) {
				const FrequencyChannel* channel = item.first;
				const std::vector<unsigned int> start_slots = item.second;
				// As many slots as targeted.
				CPPUNIT_ASSERT_EQUAL(size_t(num_slots), start_slots.size());
				// And these shouldn't equal any slots in any other channel.
				for (auto item2 : map) {
					if (*item.first != *channel) {
						for (auto slot : start_slots)
							CPPUNIT_ASSERT_EQUAL(false, std::any_of(item.second.begin(), item.second.end(), [slot](unsigned int slot2) {return slot == slot2;}));
					}
				}
			}
//			coutd.setVerbose(false);
		}

		/** Tests that the link request header fields and proposal payload are set correctly. */
		void testPrepareInitialLinkRequest() {
			auto link_request_msg = link_manager->prepareInitialRequest();
			link_request_msg.second->callback->populateLinkRequest(link_request_msg.first, link_request_msg.second);
			CPPUNIT_ASSERT_EQUAL(link_manager->default_timeout, link_request_msg.first->timeout);
			CPPUNIT_ASSERT_EQUAL(uint32_t(1), link_request_msg.first->burst_length);
			CPPUNIT_ASSERT_EQUAL(uint32_t(1), link_request_msg.first->burst_length_tx);
			CPPUNIT_ASSERT_EQUAL(link_manager->burst_offset, link_request_msg.first->burst_offset);
			const auto &proposal = link_request_msg.second->proposed_resources;
			CPPUNIT_ASSERT_EQUAL(size_t(link_manager->num_p2p_channels_to_propose), proposal.size());
			for (const auto &slots : proposal)
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->num_slots_per_p2p_channel_to_propose), slots.second.size());
		}

		void testProcessInitialRequestAllLocked() {
			auto link_request_msg = link_manager->prepareInitialRequest();
			link_request_msg.second->callback->populateLinkRequest(link_request_msg.first, link_request_msg.second);
//			coutd.setVerbose(true);
			P2PLinkManager::LinkState *state = link_manager->processInitialRequest((const L2HeaderLinkRequest*&) link_request_msg.first, (const LinkManager::LinkRequestPayload*&) link_request_msg.second);
			// Since the same user that created the request is now processing it, all resources must be locked, so none can be found viable.
			CPPUNIT_ASSERT(state->channel == nullptr);
			delete state;
//			coutd.setVerbose(false);
		}

		void testProcessInitialRequest() {
			auto link_request_msg = link_manager->prepareInitialRequest();
			link_request_msg.second->callback->populateLinkRequest(link_request_msg.first, link_request_msg.second);
//			coutd.setVerbose(true);
			TestEnvironment rx_env = TestEnvironment(partner_id, own_id, true);
			P2PLinkManager::LinkState *state = ((P2PLinkManager*) rx_env.mac_layer->getLinkManager(own_id))->processInitialRequest((const L2HeaderLinkRequest*&) link_request_msg.first, (const LinkManager::LinkRequestPayload*&) link_request_msg.second);

			CPPUNIT_ASSERT_EQUAL(state->timeout, link_request_msg.first->timeout);
			CPPUNIT_ASSERT_EQUAL(state->burst_length_tx, link_request_msg.first->burst_length_tx);
			CPPUNIT_ASSERT_EQUAL(state->burst_length, link_request_msg.first->burst_length);
			// Processor is never the link initiator.
			CPPUNIT_ASSERT_EQUAL(false, state->initiated_link);
			const FrequencyChannel *channel = state->channel;
			unsigned int slot_offset = state->slot_offset;
			CPPUNIT_ASSERT(channel != nullptr);
			CPPUNIT_ASSERT(slot_offset > 0);
			// The chosen resource should be one of the proposed ones.
			CPPUNIT_ASSERT_EQUAL(true, std::any_of(link_request_msg.second->proposed_resources.begin(), link_request_msg.second->proposed_resources.end(), [channel, slot_offset](const auto &pair){
				return *channel == *pair.first && std::any_of(pair.second.begin(), pair.second.end(), [slot_offset](unsigned int proposed_slot) {
					return slot_offset == proposed_slot;
				});
			}));

			delete state;
//			coutd.setVerbose(false);
		}

	CPPUNIT_TEST_SUITE(P2PLinkManagerTests);
		CPPUNIT_TEST(testInitialP2PSlotSelection);
		CPPUNIT_TEST(testRenewalP2PSlotSelection);
		CPPUNIT_TEST(testMultiChannelP2PSlotSelection);
		CPPUNIT_TEST(testPrepareInitialLinkRequest);
		CPPUNIT_TEST(testProcessInitialRequestAllLocked);
		CPPUNIT_TEST(testProcessInitialRequest);
	CPPUNIT_TEST_SUITE_END();
	};
}