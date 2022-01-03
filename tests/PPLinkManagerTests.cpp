//
// Created by seba on 2/18/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../PPLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class PPLinkManagerTests : public CppUnit::TestFixture {
	private:
		uint32_t planning_horizon;
		PPLinkManager *link_manager;
		MacId own_id, partner_id;
		TestEnvironment *env;
		ReservationManager *reservation_manager;

	public:
		void setUp() override {
			own_id = MacId(42);
			partner_id = MacId(43);
			env = new TestEnvironment(own_id, partner_id);
			link_manager = (PPLinkManager*) env->mac_layer->getLinkManager(partner_id);
			reservation_manager = env->mac_layer->getReservationManager();
			planning_horizon = env->planning_horizon;
		}

		void tearDown() override {
			delete env;
		}

		void testInitialP2PSlotSelection() {
			unsigned int num_channels = 1, num_slots = 3, min_offset = 2, burst_length = 5, burst_length_tx = 3;
			auto pair = link_manager->p2pSlotSelection(num_channels, num_slots, min_offset, burst_length, burst_length_tx);
			auto map = pair.first;
			CPPUNIT_ASSERT_EQUAL(size_t(num_channels), map.size());

			const FrequencyChannel* channel = map.begin()->first;
			const std::vector<unsigned int> start_offsets = map.begin()->second;
			const auto* reservation_table = reservation_manager->getReservationTable(channel);
			const auto* tx_table = reservation_manager->getTxTable();

			// At these slots the reply may arrive, so expect a receiver as well as the local ReservationTable to be locked.
			for (unsigned int t = min_offset; t < min_offset + num_slots; t++) {
				CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED), reservation_table->getReservation(t));
				CPPUNIT_ASSERT_EQUAL(true, std::any_of(reservation_manager->getRxTables().begin(), reservation_manager->getRxTables().end(), [t](ReservationTable *table) {
					return table->getReservation(t) == Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED);
				}));
				// The transmitter should *not* be locked.
				CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), tx_table->getReservation(t));
			}

			// Then for every burst until timeout, the initial slots should be TX-reserved and the latter RX-reserved.
			for (unsigned int offset : start_offsets) {
				for (unsigned int burst = 1; burst < link_manager->default_timeout + 1; burst++) { // Start at 1 due to very first burst belonging to a reply
					for (unsigned int t = 0; t < burst_length; t++) {
						unsigned int slot = offset + burst*link_manager->burst_offset + t;
						// TX reservations
						if (t < burst_length_tx) {
							// Locally locked
							CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED), reservation_table->getReservation(slot));
							// Transmitter locked
							CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED), tx_table->getReservation(slot));
						// RX reservations
						} else {
							// Locally locked
							CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED), reservation_table->getReservation(slot));
							// Receiver locked
							CPPUNIT_ASSERT_EQUAL(true, std::any_of(reservation_manager->getRxTables().begin(), reservation_manager->getRxTables().end(), [slot](ReservationTable *table) {
								return table->getReservation(slot) == Reservation(SYMBOLIC_ID_UNSET, Reservation::LOCKED);
							}));
						}
					}
				}
			}
		}

		void testClearLockedResources() {
			// Make locks.
			auto link_request_msg = link_manager->prepareRequestMessage();
			link_request_msg.second->callback->populateLinkRequest(link_request_msg.first, link_request_msg.second);
			link_manager->clearLockedResources(link_manager->lock_map);
			// *Everything* should be unlocked now.
			for (const auto* table : reservation_manager->getP2PReservationTables()) {
				for (int t = 0; t < table->getPlanningHorizon(); t++) {
					if (table->getReservation(t).isLocked())
						std::cerr << "f=" << *table->getLinkedChannel() << " t=" << t << ": " << table->getReservation(t) << std::endl;
					CPPUNIT_ASSERT_EQUAL(false, table->getReservation(t).isLocked());
				}
			}
		}

		void testMultiChannelP2PSlotSelection() {
//			coutd.setVerbose(true);
			unsigned int num_channels = 3, num_slots = 3, min_offset = 2, burst_length = 5, burst_length_tx = 3;
			auto pair = link_manager->p2pSlotSelection(num_channels, num_slots, min_offset, burst_length, burst_length_tx);
			auto map = pair.first;
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
			auto link_request_msg = link_manager->prepareRequestMessage();
			link_request_msg.second->callback->populateLinkRequest(link_request_msg.first, link_request_msg.second);
			CPPUNIT_ASSERT_EQUAL(link_manager->default_timeout, link_request_msg.first->timeout);
			CPPUNIT_ASSERT_EQUAL(uint32_t(1), link_request_msg.first->burst_length);
			CPPUNIT_ASSERT_EQUAL(uint32_t(1), link_request_msg.first->burst_length_tx);
			CPPUNIT_ASSERT_EQUAL(link_manager->burst_offset, link_request_msg.first->burst_offset);
			// Same values should've been saved in the state.
			CPPUNIT_ASSERT(link_manager->current_link_state != nullptr);
			CPPUNIT_ASSERT_EQUAL(link_manager->default_timeout, link_manager->current_link_state->timeout);
			CPPUNIT_ASSERT_EQUAL(uint32_t(1), link_manager->current_link_state->burst_length);
			CPPUNIT_ASSERT_EQUAL(uint32_t(1), link_manager->current_link_state->burst_length_tx);
			// Proposed resources should be present.
			const auto &proposal = link_request_msg.second->proposed_resources;
			CPPUNIT_ASSERT_EQUAL(size_t(link_manager->num_p2p_channels_to_propose), proposal.size());
			for (const auto &slots : proposal)
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->num_slots_per_p2p_channel_to_propose), slots.second.size());
			delete link_request_msg.first;
			delete link_request_msg.second;
		}

		void testSelectResourceFromRequestAllLocked() {
			auto link_request_msg = link_manager->prepareRequestMessage();
			link_request_msg.second->callback->populateLinkRequest(link_request_msg.first, link_request_msg.second);
			CPPUNIT_ASSERT_THROW(link_manager->selectResourceFromRequest((const L2HeaderLinkRequest*&) link_request_msg.first, (const LinkManager::LinkEstablishmentPayload*&) link_request_msg.second), std::invalid_argument);
		}

		void testSelectResourceFromRequest() {
			auto link_request_msg = link_manager->prepareRequestMessage();
			link_request_msg.second->callback->populateLinkRequest(link_request_msg.first, link_request_msg.second);
//			coutd.setVerbose(true);
			TestEnvironment rx_env = TestEnvironment(partner_id, own_id);
			PPLinkManager::LinkState *state = ((PPLinkManager*) rx_env.mac_layer->getLinkManager(own_id))->selectResourceFromRequest((const L2HeaderLinkRequest*&) link_request_msg.first, (const LinkManager::LinkEstablishmentPayload*&) link_request_msg.second);
			CPPUNIT_ASSERT_EQUAL(state->timeout, link_request_msg.first->timeout);
			CPPUNIT_ASSERT_EQUAL(state->burst_length_tx, link_request_msg.first->burst_length_tx);
			CPPUNIT_ASSERT_EQUAL(state->burst_length, link_request_msg.first->burst_length);
			// Processor is never the link initiator.
			CPPUNIT_ASSERT_EQUAL(false, state->is_link_initiator);
			const FrequencyChannel *channel = state->channel;
			unsigned int slot_offset = state->next_burst_start;
			CPPUNIT_ASSERT(channel != nullptr);
			CPPUNIT_ASSERT(slot_offset > 0);
			// The chosen resource should be one of the proposed ones.
			CPPUNIT_ASSERT_EQUAL(true, std::any_of(link_request_msg.second->proposed_resources.begin(), link_request_msg.second->proposed_resources.end(), [channel, slot_offset](const auto &pair){
				return *channel == *pair.first && std::any_of(pair.second.begin(), pair.second.end(), [slot_offset](unsigned int proposed_slot) {
					return slot_offset == proposed_slot;
				});
			}));

			delete state;
			delete link_request_msg.first;
			delete link_request_msg.second;
//			coutd.setVerbose(false);
		}

		void testTriggerLinkEstablishment() {
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, link_manager->link_status);
			link_manager->notifyOutgoing(512);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, link_manager->link_status);
			// Increment time until the link request has been sent.
			size_t num_slots = 0, max_num_slots = 100;
			while (link_manager->current_link_state == nullptr && num_slots++ < max_num_slots) {
				env->mac_layer->update(1);
				env->mac_layer->execute();
				env->mac_layer->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT(link_manager->current_link_state != nullptr);
			// Now the proposal has been populated, and so the burst starts slots should've been reserved for RX to be able to receive the reply.
			unsigned int closest_burst_start = 10000;
			for (const auto &pair : link_manager->current_link_state->scheduled_rx_slots) {
				const ReservationTable *table = reservation_manager->getReservationTable(pair.first);
				CPPUNIT_ASSERT(pair.second > 0);
				if (pair.second < closest_burst_start)
					closest_burst_start = pair.second;
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), table->getReservation(pair.second));
			}
			// And updating should also update these offsets.
			for (unsigned int t = 0; t < closest_burst_start; t++)
				env->mac_layer->update(1);
			unsigned int closest_burst_start2 = 10000;
			for (const auto &pair : link_manager->current_link_state->scheduled_rx_slots) {
				const ReservationTable *table = reservation_manager->getReservationTable(pair.first);
				CPPUNIT_ASSERT(pair.second >= 0);
				if (pair.second < closest_burst_start2)
					closest_burst_start2 = pair.second;
				CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), table->getReservation(pair.second));
			}
			CPPUNIT_ASSERT_EQUAL(uint32_t(0), closest_burst_start2);
		}

		void testReplyToRequest() {
//			coutd.setVerbose(true);
			// Prepare request from another user.
			TestEnvironment rx_env = TestEnvironment(partner_id, own_id);
			auto link_request_msg = ((PPLinkManager*) rx_env.mac_layer->getLinkManager(own_id))->prepareRequestMessage();
			link_request_msg.second->callback->populateLinkRequest(link_request_msg.first, link_request_msg.second);

			// Right now, the link should be unestablished.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, link_manager->link_status);
			CPPUNIT_ASSERT(link_manager->current_link_state == nullptr);
			CPPUNIT_ASSERT(link_manager->current_channel == nullptr);
			CPPUNIT_ASSERT(link_manager->current_reservation_table == nullptr);

			// Now process the request.
			link_manager->processLinkRequestMessage((const L2Header*&) link_request_msg.first, (const L2Packet::Payload*&) link_request_msg.second, partner_id);
			// Now, the link should be being established.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_data_tx, link_manager->link_status);
			CPPUNIT_ASSERT(link_manager->current_link_state != nullptr);
			CPPUNIT_ASSERT(link_manager->current_channel != nullptr);
			CPPUNIT_ASSERT(link_manager->current_reservation_table != nullptr);
			CPPUNIT_ASSERT_EQUAL(size_t(1), link_manager->current_link_state->scheduled_link_replies.size());
			// Within one P2P frame there should just be the transmission of the reply scheduled.
			size_t num_tx = 0;
			for (size_t t = 0; t < link_manager->burst_offset; t++) {
				const Reservation &res = link_manager->current_reservation_table->getReservation(t);
				if (res.isTx()) {
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), res);
					num_tx++;
				} else
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), res);
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), num_tx);
			// And the first data exchange should be expected one burst later.
			size_t num_rx = 0;
			for (size_t t = link_manager->burst_offset; t < planning_horizon; t++) {
				const Reservation &res = link_manager->current_reservation_table->getReservation(t);
				if (res.isRx()) {
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), res);
					num_rx++;
				} else
					CPPUNIT_ASSERT(res.isLocked() || res.isIdle());
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), num_rx);
			delete link_request_msg.first;
			delete link_request_msg.second;
		}

		/** Tests that scheduled link replies' offsets are decremented each slot. */
		void testDecrementControlMessageOffsets() {
			// Schedule a reply.
			testReplyToRequest();
			CPPUNIT_ASSERT(link_manager->current_link_state != nullptr);
			CPPUNIT_ASSERT_EQUAL(size_t(1), link_manager->current_link_state->scheduled_link_replies.size());
			auto &reply_reservation = link_manager->current_link_state->scheduled_link_replies.at(0);
			// Reply should encode a single slot.
			CPPUNIT_ASSERT_EQUAL(size_t(1), reply_reservation.getPayload()->proposed_resources.begin()->second.size());
			// Some time in the future.
			CPPUNIT_ASSERT(reply_reservation.getPayload()->proposed_resources.begin()->second.at(0) > 0);

			// Now increment time.
			CPPUNIT_ASSERT(reply_reservation.getRemainingOffset() > 0);
			size_t num_slots = 0, max_num_slots = reply_reservation.getRemainingOffset();
			while (reply_reservation.getRemainingOffset() > 0 && num_slots++ < max_num_slots)
				env->mac_layer->update(1);
			CPPUNIT_ASSERT_EQUAL(num_slots, max_num_slots);
			CPPUNIT_ASSERT_EQUAL(uint32_t(0), reply_reservation.getRemainingOffset());
			CPPUNIT_ASSERT_EQUAL(size_t(1), reply_reservation.getPayload()->proposed_resources.begin()->second.size());
			// The slot offset should've also been decreased.
			CPPUNIT_ASSERT_EQUAL(uint32_t(0), reply_reservation.getPayload()->proposed_resources.begin()->second.at(0));
			// Incrementing once more should throw an exception, as the control message would've been missed.
			CPPUNIT_ASSERT_THROW(env->mac_layer->update(1), std::invalid_argument);
		}

		void testScheduleBurst() {
			link_manager->assign(reservation_manager->getP2PFreqChannels().at(0));
			unsigned int burst_start = 5, burst_length = 5, burst_length_tx = 3;
			link_manager->scheduleBurst(burst_start, burst_length, burst_length_tx, partner_id, link_manager->current_reservation_table, true);
			size_t num_tx = 0, num_rx = 0;
			for (size_t t = 0; t < burst_length_tx; t++) {
				if (t == 0)
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), link_manager->current_reservation_table->getReservation(burst_start + t));
				else
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX_CONT), link_manager->current_reservation_table->getReservation(burst_start + t));
				num_tx++;
			}
			for (size_t t = 0; t < burst_length - burst_length_tx; t++) {
				if (t == 0)
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), link_manager->current_reservation_table->getReservation(burst_start + burst_length_tx + t));
				else
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX_CONT), link_manager->current_reservation_table->getReservation(burst_start + burst_length_tx + t));
				num_rx++;
			}

			CPPUNIT_ASSERT_EQUAL(size_t(burst_length_tx), num_tx);
			CPPUNIT_ASSERT_EQUAL(size_t(burst_length - burst_length_tx), num_rx);
		}

		void testSendScheduledReply() {
			// Schedule a reply.
			testReplyToRequest();
//			coutd.setVerbose(true);
			CPPUNIT_ASSERT(link_manager->current_link_state != nullptr);
			CPPUNIT_ASSERT_EQUAL(size_t(1), link_manager->current_link_state->scheduled_link_replies.size());
			auto &reply_reservation = link_manager->current_link_state->scheduled_link_replies.at(0);

			// Now increment time.
			CPPUNIT_ASSERT(reply_reservation.getRemainingOffset() > 0);
			size_t num_slots = 0, max_num_slots = reply_reservation.getRemainingOffset();
			while (reply_reservation.getRemainingOffset() > 0 && num_slots++ < max_num_slots) {
				env->mac_layer->update(1);
				env->mac_layer->execute();
				env->mac_layer->onSlotEnd();
			}

			// Now the scheduled reply should've been sent.
			CPPUNIT_ASSERT_EQUAL(true, link_manager->current_link_state->scheduled_link_replies.empty());
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());
		}

		void testProcessInitialLinkReply() {
//			coutd.setVerbose(true);
			// Prepare request.
			TestEnvironment rx_env = TestEnvironment(partner_id, own_id);
			link_manager->notifyOutgoing(512);
			auto link_request_msg = link_manager->prepareRequestMessage();
			link_request_msg.second->callback->populateLinkRequest(link_request_msg.first, link_request_msg.second);
			// Receive request.
			((PPLinkManager*) rx_env.mac_layer->getLinkManager(own_id))->processLinkRequestMessage((const L2Header*&) link_request_msg.first, (const L2Packet::Payload*&) link_request_msg.second, own_id);
			// Send the reply.
			size_t num_slots = 0, max_num_slots = 100;
			while (!((PPLinkManager*) rx_env.mac_layer->getLinkManager(own_id))->current_link_state->scheduled_link_replies.empty() && num_slots++ < max_num_slots) {
				rx_env.mac_layer->update(1);
				rx_env.mac_layer->execute();
				rx_env.mac_layer->onSlotEnd();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(true, ((PPLinkManager*) rx_env.mac_layer->getLinkManager(own_id))->current_link_state->scheduled_link_replies.empty());
			CPPUNIT_ASSERT_EQUAL(size_t(1), rx_env.phy_layer->outgoing_packets.size());
			L2Packet *link_reply = rx_env.phy_layer->outgoing_packets.at(0);
			int reply_index = link_reply->getReplyIndex();
			CPPUNIT_ASSERT(reply_index > -1);
			// Locally some RX reservations should exist, later link resources should be locked and everything else idle.
			size_t num_rx_res = 0;
			for (const auto *channel : reservation_manager->getP2PFreqChannels()) {
				const ReservationTable *table = reservation_manager->getReservationTable(channel);
				for (size_t t = 0; t < planning_horizon; t++) {
					if (table->getReservation(t).isRx())
						num_rx_res++;
					else
						CPPUNIT_ASSERT(table->getReservation(t).isIdle() || table->getReservation(t).isLocked());
				}
			}
			CPPUNIT_ASSERT(num_rx_res > 0);
			// Process the link reply.
//			coutd.setVerbose(true);
			link_manager->processLinkReplyMessage((const L2HeaderLinkEstablishmentReply*&) link_reply->getHeaders().at(reply_index), (const LinkManager::LinkEstablishmentPayload*&) link_reply->getPayloads().at(reply_index), ((L2HeaderBase*) link_reply->getHeaders().at(0))->src_id);
//			coutd.setVerbose(false);
			// Transmission bursts should've been saved now.
			const ReservationTable *table = link_manager->current_reservation_table;
			for (unsigned int burst = 1; burst < link_manager->default_timeout; burst++) {
				unsigned int burst_start_offset = burst*link_manager->burst_offset;
				for (size_t t = 0; t < link_manager->current_link_state->burst_length; t++) {
					if (t == 0)
						CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX), table->getReservation(burst_start_offset + t));
					else
						CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::TX_CONT), table->getReservation(burst_start_offset + t));
				}
			}
			// Nothing but these transmission reservations should exist, i.e. RX reservations should've been cleared.
			for (const auto *channel : reservation_manager->getP2PFreqChannels()) {
				const ReservationTable *other_table = reservation_manager->getReservationTable(channel);
				if (other_table == table) {
					for (size_t t = 0; t < planning_horizon; t++)
						CPPUNIT_ASSERT(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE) == other_table->getReservation(t) || Reservation(partner_id, Reservation::TX) == other_table->getReservation(t));
				} else
					for (size_t t = 0; t < planning_horizon; t++)
						CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), other_table->getReservation(t));
			}
			delete link_request_msg.first;
			delete link_request_msg.second;
		}

		void testLinkRequestSize() {
			testProcessInitialLinkReply();
			auto request_msg = link_manager->prepareRequestMessage();
			PPLinkManager::ControlMessageReservation msg = PPLinkManager::ControlMessageReservation(0, (L2Header*&) request_msg.first, (LinkManager::LinkEstablishmentPayload*&) request_msg.second);
			L2HeaderLinkRequest request = L2HeaderLinkRequest();
			CPPUNIT_ASSERT_EQUAL(request.getBits(), msg.getHeader()->getBits());
			CPPUNIT_ASSERT_EQUAL(uint32_t(0), msg.getPayload()->getBits());
			msg.getPayload()->callback->populateLinkRequest((L2HeaderLinkRequest*&) msg.getHeader(), msg.getPayload());
			CPPUNIT_ASSERT(msg.getPayload()->getBits() > 0);
			delete request_msg.first;
			delete request_msg.second;
		}

		void testPrepareRequestMessageMemoryLeak() {
			auto pair = link_manager->prepareRequestMessage();
			std::vector<PPLinkManager::ControlMessageReservation> vec;
			link_manager->current_link_state = new PPLinkManager::LinkState(10, 10, 10);
			delete pair.first;
			delete pair.second;
		}		

		void testFreeP2PSlotSelectionLocks() {
			unsigned int num_channels = 2, num_slots = 3, min_offset = 1, burst_length = 3, burst_length_tx = 3;
			auto pair = link_manager->p2pSlotSelection(num_channels, num_slots, min_offset, burst_length, burst_length_tx);
			link_manager->clearLockedResources(pair.second);
			const std::vector<uint64_t> freqs = {env->p2p_freq_1, env->p2p_freq_2, env->p2p_freq_3};
			for (auto freq : freqs) 
				for (size_t t = 0; t < planning_horizon; t++) 
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(freq))->getReservation(t));				
		}

		void testFreeP2PSlotSelectionLocksAfterTime() {
			link_manager->onSlotStart(1);
			unsigned int num_channels = 2, num_slots = 3, min_offset = 1, burst_length = 3, burst_length_tx = 3;
			auto pair = link_manager->p2pSlotSelection(num_channels, num_slots, min_offset, burst_length, burst_length_tx);			
			link_manager->onSlotEnd();
			for (size_t t = 0; t < 2; t++) {
				link_manager->onSlotStart(1);
				link_manager->onSlotEnd();
			}
			link_manager->clearLockedResources(pair.second);
			const std::vector<uint64_t> freqs = {env->p2p_freq_1, env->p2p_freq_2, env->p2p_freq_3};
			for (auto freq : freqs) 
				for (size_t t = 0; t < planning_horizon; t++) 
					CPPUNIT_ASSERT_EQUAL(Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE), reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(freq))->getReservation(t));				
		}

		/** For very large data requests, the number of required slots may exceed the frame length. In such cases, a fair distribution of slots to either communication partner is required. */
		void testFairTxRxDistribution() {
			unsigned int tx_slots_me = link_manager->burst_offset / 2;
			unsigned int tx_slots_you = link_manager->burst_offset / 2;
			auto pair = link_manager->getTxRxDistribution(tx_slots_me, tx_slots_you);
			unsigned int burst_length_tx = pair.first, burst_length = pair.second, burst_length_rx = burst_length - burst_length_tx;			
			CPPUNIT_ASSERT_EQUAL(burst_length_tx, burst_length_rx);
			CPPUNIT_ASSERT_EQUAL(burst_length_tx + burst_length_rx, burst_length);
		}

		void testLessBusyTxRxDistribution() {
			unsigned int tx_slots_me = link_manager->burst_offset;
			unsigned int tx_slots_you = 0;
			auto pair = link_manager->getTxRxDistribution(tx_slots_me, tx_slots_you);
			unsigned int burst_length_tx = pair.first, burst_length = pair.second, burst_length_rx = burst_length - burst_length_tx;			
			CPPUNIT_ASSERT_EQUAL(burst_length_tx, link_manager->burst_offset);
			CPPUNIT_ASSERT_EQUAL(burst_length_tx + burst_length_rx, burst_length);
		}

		void testEvenLessBusyTxRxDistribution() {
			unsigned int tx_slots_me = link_manager->burst_offset - 3;
			unsigned int tx_slots_you = 2;
			auto pair = link_manager->getTxRxDistribution(tx_slots_me, tx_slots_you);
			unsigned int burst_length_tx = pair.first, burst_length = pair.second, burst_length_rx = burst_length - burst_length_tx;			
			CPPUNIT_ASSERT_EQUAL(burst_length_tx, link_manager->burst_offset - 3);
			CPPUNIT_ASSERT_EQUAL(burst_length_rx, uint(2));
			CPPUNIT_ASSERT_EQUAL(burst_length_tx + burst_length_rx, link_manager->burst_offset - 3 + 2);
		}

		void testLargeTxRxDistribution() {
			unsigned int tx_slots_me = link_manager->burst_offset + 5;
			unsigned int tx_slots_you = link_manager->burst_offset;
			auto pair = link_manager->getTxRxDistribution(tx_slots_me, tx_slots_you);
			unsigned int burst_length_tx = pair.first, burst_length = pair.second, burst_length_rx = burst_length - burst_length_tx;			
			CPPUNIT_ASSERT_GREATER(burst_length_rx, burst_length_tx);				
			CPPUNIT_ASSERT_EQUAL(link_manager->burst_offset, burst_length_tx + burst_length_rx);
		}

	CPPUNIT_TEST_SUITE(PPLinkManagerTests);
		CPPUNIT_TEST(testInitialP2PSlotSelection);
		CPPUNIT_TEST(testClearLockedResources);
		CPPUNIT_TEST(testMultiChannelP2PSlotSelection);
		CPPUNIT_TEST(testPrepareInitialLinkRequest);
		CPPUNIT_TEST(testSelectResourceFromRequestAllLocked);
		CPPUNIT_TEST(testSelectResourceFromRequest);
		CPPUNIT_TEST(testTriggerLinkEstablishment);
		CPPUNIT_TEST(testReplyToRequest);
		CPPUNIT_TEST(testDecrementControlMessageOffsets);
		CPPUNIT_TEST(testScheduleBurst);
		CPPUNIT_TEST(testSendScheduledReply);
		CPPUNIT_TEST(testProcessInitialLinkReply);
		CPPUNIT_TEST(testLinkRequestSize);
		CPPUNIT_TEST(testPrepareRequestMessageMemoryLeak);		
		CPPUNIT_TEST(testFreeP2PSlotSelectionLocks);				
		CPPUNIT_TEST(testFreeP2PSlotSelectionLocksAfterTime);			
		CPPUNIT_TEST(testFairTxRxDistribution);	
		CPPUNIT_TEST(testLessBusyTxRxDistribution);	
		CPPUNIT_TEST(testEvenLessBusyTxRxDistribution);	
		CPPUNIT_TEST(testLargeTxRxDistribution);			
	CPPUNIT_TEST_SUITE_END();
	};
}