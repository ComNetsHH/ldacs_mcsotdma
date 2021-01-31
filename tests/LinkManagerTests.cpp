//
// Created by Sebastian Lindner on 18.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../LinkManager.hpp"
#include "../BCLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * The LinkManager is the core component of the IntAirNet' LDACS MAC.
	 * These tests aim at one side of the communication link, e.g. the preparation of a request and testing its contents.
	 * Tests that involve both TX *and* RX are put into SystemTests instead.
	 */
	class LinkManagerTests : public CppUnit::TestFixture {
	private:
		TestEnvironment* env;

		LinkManager* link_manager;
		ReservationManager* reservation_manager;
		MacId own_id;
		MacId communication_partner_id;
		uint32_t planning_horizon;
		uint64_t center_frequency1, center_frequency2, center_frequency3, bc_frequency, bandwidth;
		unsigned long num_bits_going_out = 800 * 100;
		MACLayer* mac;
		ARQLayer* arq_layer;
		RLCLayer* rlc_layer;
		PHYLayer* phy_layer;
		NetworkLayer* net_layer;

	public:
		void setUp() override {
			own_id = MacId(42);
			communication_partner_id = MacId(43);
			env = new TestEnvironment(own_id, communication_partner_id);

			planning_horizon = env->planning_horizon;
			center_frequency1 = env->center_frequency1;
			center_frequency2 = env->center_frequency2;
			center_frequency3 = env->center_frequency3;
			bc_frequency = env->bc_frequency;
			bandwidth = env->bandwidth;

			phy_layer = env->phy_layer;
			mac = env->mac_layer;
			reservation_manager = mac->reservation_manager;
			link_manager = mac->getLinkManager(communication_partner_id);
			arq_layer = env->arq_layer;
			net_layer = env->net_layer;
			rlc_layer = env->rlc_layer;
		}

		void tearDown() override {
			delete env;
		}

		/**
		 * Tests the updateTrafficEstimate() directly.
		 */
		void testTrafficEstimate() {
			CPPUNIT_ASSERT_EQUAL(0.0, link_manager->getCurrentTrafficEstimate());
			unsigned int initial_bits = 10;
			unsigned int num_bits = initial_bits;
			double sum = 0;
			// Fill up the window.
			for (size_t i = 0; i < link_manager->traffic_estimate.values.size(); i++) {
				link_manager->updateTrafficEstimate(num_bits);
				sum += num_bits;
				num_bits += initial_bits;
				CPPUNIT_ASSERT_EQUAL(sum / (i + 1), link_manager->getCurrentTrafficEstimate());
			}
			// Now it's full, so the next input will kick out the first value.
			link_manager->updateTrafficEstimate(num_bits);
			sum -= initial_bits;
			sum += num_bits;
			CPPUNIT_ASSERT_EQUAL(sum / (link_manager->traffic_estimate.values.size()), link_manager->getCurrentTrafficEstimate());
		}

		/**
		 * Tests updating the traffic estimate over a number of slots.
		 */
		void testTrafficEstimateOverTimeslots() {
			double expected_estimate = 0.0;
			CPPUNIT_ASSERT_EQUAL(expected_estimate, link_manager->getCurrentTrafficEstimate());
			unsigned int bits_to_send = 1024;
			expected_estimate = bits_to_send;
			// Fill one moving average window...
			for (size_t i = 0; i < link_manager->traffic_estimate.values.size(); i++) {
				mac->notifyOutgoing(bits_to_send, communication_partner_id);
				mac->update(1);
				CPPUNIT_ASSERT_EQUAL(expected_estimate, link_manager->getCurrentTrafficEstimate());
			}
			// ... and then shrink it per slot by not notifying about any new data
			for (size_t i = 0; i < link_manager->traffic_estimate.values.size(); i++) {
				mac->update(1);
				expected_estimate -= ((double) bits_to_send / ((double) link_manager->traffic_estimate.values.size()));
				// Comparing doubles...
				CPPUNIT_ASSERT(link_manager->getCurrentTrafficEstimate() > expected_estimate - 1 && link_manager->getCurrentTrafficEstimate() < expected_estimate + 1);
			}
		}

		void testNewLinkEstablishment() {
			// It must a be a P2P link.
			CPPUNIT_ASSERT(own_id != SYMBOLIC_LINK_ID_BROADCAST && own_id != SYMBOLIC_LINK_ID_BEACON);
			// Initially the link should not be established.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, link_manager->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(size_t(0), rlc_layer->control_message_injections.size());
			// Now inform the LinkManager of new data for this link.
			link_manager->notifyOutgoing(num_bits_going_out);
			// The RLC should've received a link request.
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer->control_message_injections.size());
			CPPUNIT_ASSERT(rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).at(0)->getRequestIndex() > -1);
			CPPUNIT_ASSERT_EQUAL(size_t(3), rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).at(0)->getHeaders().size());
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).at(0)->getHeaders().at(0)->frame_type);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::broadcast, rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).at(0)->getHeaders().at(1)->frame_type);
			CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).at(0)->getHeaders().at(2)->frame_type);
			// And the LinkManager status should've updated.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, link_manager->link_establishment_status);
		}

		void testComputeProposal() {
			testNewLinkEstablishment();
			L2Packet* request = rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).at(0);
			LinkManagementEntity::ProposalPayload* proposal = link_manager->lme->p2pSlotSelection();
			CPPUNIT_ASSERT(request->getRequestIndex() > -1);

			// Should've considered several distinct frequency channels.
			CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->num_proposed_channels), proposal->proposed_resources.size());
			for (auto it = proposal->proposed_resources.begin(); it != proposal->proposed_resources.end(); it++) {
				if (it == proposal->proposed_resources.begin()) // start at 2nd item
					continue;
				auto it_cpy = it;
				it_cpy--;
				const FrequencyChannel* channel0 = it_cpy->first;
				const FrequencyChannel* channel1 = it->first;
				CPPUNIT_ASSERT(channel1->getCenterFrequency() != channel0->getCenterFrequency());
			}

			// Should've considered a number of candidate slots per frequency channel.
			size_t total = 0;
			for (auto item : proposal->proposed_resources) {
				// Since all are idle, we should've found the target number each time.
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->num_proposed_slots), item.second.size());
				total += item.second.size();
			}
			// and so the grand total should be the number of proposed slots times the number of proposed channels.
			CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->num_proposed_channels * link_manager->lme->num_proposed_slots), total);

			// To have a look...
//				coutd.setVerbose(true);
//				for (unsigned int slot : proposal->proposed_slots)
//					coutd << slot << " ";
//				coutd << std::endl;
//				coutd.setVerbose(false);

			delete proposal;
		}

		void testNewLinkRequest() {
			CPPUNIT_ASSERT(link_manager->link_establishment_status == LinkManager::link_not_established);
			link_manager->lme->establishLink();
			CPPUNIT_ASSERT(link_manager->link_establishment_status == LinkManager::awaiting_reply);
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer->control_message_injections.size());
			L2Packet* request = rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).at(0);
			CPPUNIT_ASSERT(rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).at(0)->getRequestIndex() > -1);
			CPPUNIT_ASSERT_EQUAL(size_t(3), request->getHeaders().size());
			CPPUNIT_ASSERT(request->getHeaders().at(0)->frame_type == L2Header::base);
			CPPUNIT_ASSERT(request->getHeaders().at(1)->frame_type == L2Header::broadcast);
			CPPUNIT_ASSERT(request->getHeaders().at(2)->frame_type == L2Header::link_establishment_request);
		}

		void testTransmissionSlotOnUnestablishedLink() {
			bool exception_thrown = false;
			try {
				link_manager->onTransmissionBurst(1);
			} catch (const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
		}

		void testOnTransmissionSlot() {
			CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
			// Transmission slots should only occur for established links.
			link_manager->link_establishment_status = LinkManager::link_established;
			L2Packet* packet = link_manager->onTransmissionBurst(1);
			CPPUNIT_ASSERT(packet != nullptr);
			delete packet;
		}

		void testSetBaseHeader() {
			L2HeaderBase header = L2HeaderBase();
			link_manager->setHeaderFields(&header);
			CPPUNIT_ASSERT_EQUAL(own_id, header.icao_src_id);
			CPPUNIT_ASSERT_EQUAL(link_manager->lme->tx_offset, header.offset);
			CPPUNIT_ASSERT_EQUAL(link_manager->lme->tx_burst_num_slots, header.length_next);
			CPPUNIT_ASSERT_EQUAL(link_manager->lme->tx_timeout, header.timeout);
		}

		void testSetBeaconHeader() {
			L2HeaderBeacon header = L2HeaderBeacon();
			LinkManager broadcast_link_manager = LinkManager(SYMBOLIC_LINK_ID_BROADCAST, reservation_manager, mac);
			// Shouldn't try to set a beacon header with a P2P link manager.
			bool exception_thrown = false;
			try {
				link_manager->setHeaderFields(&header);
			} catch (const std::exception& e) {
				exception_thrown = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
		}

		void testSetUnicastHeader() {
			L2HeaderUnicast header = L2HeaderUnicast(L2Header::FrameType::unicast);
			link_manager->link_establishment_status = LinkManager::link_established;
			link_manager->setHeaderFields(&header);
			CPPUNIT_ASSERT_EQUAL(communication_partner_id, header.icao_dest_id);
		}

		void testSetRequestHeader() {
			L2HeaderLinkEstablishmentRequest header = L2HeaderLinkEstablishmentRequest();
			link_manager->setHeaderFields(&header);
			CPPUNIT_ASSERT_EQUAL(communication_partner_id, header.icao_dest_id);
		}

		void testProcessIncomingBase() {
			// Assign a reservation table.
			ReservationTable* table = reservation_manager->p2p_reservation_tables.at(0);
			link_manager->current_reservation_table = table;
//				coutd.setVerbose(true);
			// Prepare incoming packet.
			auto* packet = new L2Packet();
			unsigned int offset = 5, length_next = 2, timeout = 3;
			auto* base_header = new L2HeaderBase(communication_partner_id, offset, length_next, timeout);
			packet->addPayload(base_header, nullptr);
			// Have the LinkManager process it.
			link_manager->receiveFromLower(packet);
			// Ensure that the slots were marked.
			for (size_t i = 0; i < timeout; i++) {
				const Reservation& reservation = table->getReservation((i + 1) * offset);
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation.getTarget());
				CPPUNIT_ASSERT_EQUAL(true, reservation.isRx());
				CPPUNIT_ASSERT_EQUAL(length_next - 1, reservation.getNumRemainingSlots());
				for (unsigned int j = 1; j < length_next; j++) {
					const Reservation& cont_reservation = table->getReservation((i + 1) * offset + j);
					CPPUNIT_ASSERT_EQUAL(communication_partner_id, cont_reservation.getTarget());
					CPPUNIT_ASSERT_EQUAL(true, cont_reservation.isRx());
					CPPUNIT_ASSERT_EQUAL(length_next - 1 - j, cont_reservation.getNumRemainingSlots());
				}

			}
		}

		void testProcessIncomingLinkEstablishmentRequest() {
			// Assign a reservation table.
			ReservationTable* table = reservation_manager->p2p_reservation_tables.at(0);
			link_manager->current_reservation_table = table;
			// Prepare a link establishment request by our communication partner.
			MACLayer other_mac = MACLayer(communication_partner_id, planning_horizon);
			other_mac.setUpperLayer(arq_layer);
			other_mac.setLowerLayer(phy_layer);
			other_mac.reservation_manager->setTransmitterReservationTable(
					phy_layer->getTransmitterReservationTable());
			for (ReservationTable* table : phy_layer->getReceiverReservationTables())
				other_mac.reservation_manager->addReceiverReservationTable(table);
			other_mac.reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
			other_mac.reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
			other_mac.reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
			other_mac.reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
			LinkManager other_link_manager = LinkManager(own_id, other_mac.reservation_manager, &other_mac);
//				coutd.setVerbose(true);
			L2Packet* request = other_link_manager.lme->prepareRequest();
			request->getPayloads().at(1) = other_link_manager.lme->p2pSlotSelection();
			// The number of proposed channels should be adequate.
			CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->num_proposed_channels), ((LinkManagementEntity::ProposalPayload*) request->getPayloads().at(1))->proposed_resources.size());
			auto header = (L2HeaderLinkEstablishmentRequest*) request->getHeaders().at(1);
			auto body = (LinkManagementEntity::ProposalPayload*) request->getPayloads().at(1);
			auto viable_candidates = link_manager->lme->findViableCandidatesInRequest(header, body);
			// And all slots should be viable.
			CPPUNIT_ASSERT_EQUAL((size_t) link_manager->lme->num_proposed_channels * link_manager->lme->num_proposed_slots, viable_candidates.size());
//				coutd.setVerbose(false);
		}

		void testProcessIncomingUnicast() {
			// Assign a reservation table.
			ReservationTable* table = reservation_manager->p2p_reservation_tables.at(0);
			link_manager->current_reservation_table = table;
			// Set this link as established.
			// When we receive a packet intended for us...
			L2Packet* unicast_packet_intended_for_us = rlc_layer->requestSegment(phy_layer->getCurrentDatarate(), own_id);
			auto* header_for_us = (L2HeaderUnicast*) unicast_packet_intended_for_us->getHeaders().at(1);
			L2Packet::Payload* payload_for_us = unicast_packet_intended_for_us->getPayloads().at(1);
			CPPUNIT_ASSERT(header_for_us != nullptr);
			CPPUNIT_ASSERT(payload_for_us != nullptr);
			// Right now the link is not established, which should trigger an error.
			bool exception_occurred = false;
			try {
				link_manager->processIncomingUnicast(header_for_us, payload_for_us);
			} catch (const std::runtime_error& e) {
				exception_occurred = true;
			}
			CPPUNIT_ASSERT_EQUAL(true, exception_occurred);
			// So set it to established and try again.
			link_manager->link_establishment_status = LinkManager::link_established;
			link_manager->processIncomingUnicast(header_for_us, payload_for_us);
			// ... then they should just remain for processing on the upper layers.
			CPPUNIT_ASSERT(header_for_us != nullptr);
			CPPUNIT_ASSERT(payload_for_us != nullptr);

			// When we receive a packet *not* intended for us...
			L2Packet* unicast_packet_not_intended_for_us = rlc_layer->requestSegment(phy_layer->getCurrentDatarate(), own_id);
			auto* header_not_for_us = ((L2HeaderUnicast*) unicast_packet_not_intended_for_us->getHeaders().at(1));
			header_not_for_us->icao_dest_id = communication_partner_id;
			L2Packet::Payload* payload_not_for_us = unicast_packet_not_intended_for_us->getPayloads().at(1);
			CPPUNIT_ASSERT(unicast_packet_not_intended_for_us->getHeaders().at(1) != nullptr);
			CPPUNIT_ASSERT(payload_not_for_us != nullptr);
			link_manager->processIncomingUnicast((L2HeaderUnicast*&) unicast_packet_not_intended_for_us->getHeaders().at(1), payload_not_for_us);
			// ... then they should be deleted s.t. upper layers don't attempt to process them.
			CPPUNIT_ASSERT(unicast_packet_not_intended_for_us->getHeaders().at(1) == nullptr);
			CPPUNIT_ASSERT(payload_not_for_us == nullptr);
		}

		void testPrepareLinkEstablishmentRequest() {
			L2Packet* request = link_manager->lme->prepareRequest();
			CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, request->getDestination());
			CPPUNIT_ASSERT_EQUAL(own_id, request->getOrigin());
			delete request;
			link_manager->link_establishment_status = LinkManager::link_established;
			request = link_manager->lme->prepareRequest();
			CPPUNIT_ASSERT_EQUAL(communication_partner_id, request->getDestination());
			CPPUNIT_ASSERT_EQUAL(own_id, request->getOrigin());
			delete request;
		}

		void testPrepareLinkReply() {
			L2Packet* reply = link_manager->lme->prepareReply(communication_partner_id);
			CPPUNIT_ASSERT_EQUAL(communication_partner_id, reply->getDestination());
			CPPUNIT_ASSERT_EQUAL(own_id, reply->getOrigin());
		}

		void testReplyToRequest() {
			// Assign as BC link manager.
			LinkManager* bc_manager = mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			// Prepare a link establishment request.
			TestEnvironment env2 = TestEnvironment(communication_partner_id, own_id);
			MACLayer& other_mac = *env2.mac_layer;
			LinkManager& other_link_manager = *other_mac.getLinkManager(own_id);
			L2Packet* request = other_link_manager.lme->prepareRequest();
			CPPUNIT_ASSERT(request->getRequestIndex() > -1);
			request->getPayloads().at(request->getRequestIndex()) = other_link_manager.lme->p2pSlotSelection();
//			coutd.setVerbose(true);
			coutd << request->getOrigin() << std::endl;
			// Receive it on the BC.
			mac->receiveFromLower(request, bc_frequency);
			coutd.setVerbose(false);
			// Fetch the now-instantiated P2P manager.
			LinkManager* p2p_manager = mac->getLinkManager(communication_partner_id);
			// And increment time until it has sent the reply.
			CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
			size_t num_slots = 0, num_slots_max = 100;
			while (!p2p_manager->lme->scheduled_replies.empty() && num_slots++ < num_slots_max) {
				mac->update(1);
				mac->execute();
			}
			CPPUNIT_ASSERT(num_slots < num_slots_max);
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());
			L2Packet* reply = phy_layer->outgoing_packets.at(0);
			CPPUNIT_ASSERT_EQUAL(own_id, reply->getOrigin());
			CPPUNIT_ASSERT_EQUAL(communication_partner_id, reply->getDestination());
			// Link establishment status should've been updated.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_data_tx, p2p_manager->link_establishment_status);
//			coutd.setVerbose(false);
		}

		void testLocking() {
			// Compute one request.
			L2Packet* request1 = link_manager->lme->prepareRequest();
			request1->getPayloads().at(1) = link_manager->lme->p2pSlotSelection();
			// And another one.
			L2Packet* request2 = link_manager->lme->prepareRequest();
			request2->getPayloads().at(1) = link_manager->lme->p2pSlotSelection();
			// Because the first proposed slots have been locked, they shouldn't be the same as the next.
			auto* proposal1 = (LinkManagementEntity::ProposalPayload*) request1->getPayloads().at(1);
			auto* proposal2 = (LinkManagementEntity::ProposalPayload*) request2->getPayloads().at(1);
			// We have a sufficiently large planning horizon s.t. the frequency channels can be the same.
			CPPUNIT_ASSERT_EQUAL(proposal1->proposed_resources.size(), proposal2->proposed_resources.size());
			for (const auto& item : proposal1->proposed_resources)
				CPPUNIT_ASSERT(proposal2->proposed_resources.find(item.first) != proposal2->proposed_resources.end());
			// But the slots mustn't be the same.
			for (const auto& item : proposal1->proposed_resources) {
				const FrequencyChannel* channel = item.first;
				const std::vector<unsigned int> slots1 = item.second, slots2 = proposal2->proposed_resources[channel];
				for (int32_t slot1 : slots1) { // Any out of the first proposed slots...
					for (int32_t slot2 : slots2) // can't equal any of the others.
						CPPUNIT_ASSERT(slot1 != slot2);
				}
			}

		}

		/**
		 * Tests that reservations are set correctly after a link request has been sent.
		 */
		void testReservationsAfterRequest() {
//			    coutd.setVerbose(true);

			// No need to schedule additional broadcast slots after sending the request.
			rlc_layer->should_there_be_more_data = false;
			// Injections into RLC should trigger notifications down to the corresponding LinkManager.
			arq_layer->should_forward = true;
			auto* bc_link_manager = (BCLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			CPPUNIT_ASSERT_EQUAL(false, bc_link_manager->broadcast_slot_scheduled);

			// Trigger link establishment.
			CPPUNIT_ASSERT_EQUAL(size_t(0), rlc_layer->control_message_injections.size());
			mac->notifyOutgoing(1024, communication_partner_id);
			// Request should've been injected.
			CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer->control_message_injections.size());
			CPPUNIT_ASSERT(rlc_layer->control_message_injections.at(SYMBOLIC_LINK_ID_BROADCAST).at(0)->getRequestIndex() > -1);
			// Broadcast LinkManager should've been notified.
			CPPUNIT_ASSERT_EQUAL(true, bc_link_manager->broadcast_slot_scheduled);

			// Increment time until the reply has been sent.
			CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
			size_t num_slots = 0, max_num_slots = 10;
			while (bc_link_manager->broadcast_slot_scheduled && num_slots++ < max_num_slots) {
				mac->update(1);
				mac->execute();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(false, bc_link_manager->broadcast_slot_scheduled);

			// Request should've been sent.
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());

			// Now RX reservations should've been made at all proposed slots.
			L2Packet* request = phy_layer->outgoing_packets.at(0);
			auto* request_header = (L2HeaderLinkEstablishmentRequest*) request->getHeaders().at(1);
			auto* request_body = (LinkManagementEntity::ProposalPayload*) request->getPayloads().at(request->getRequestIndex());
			CPPUNIT_ASSERT_EQUAL(size_t(request_body->target_num_channels), request_body->proposed_resources.size());
			size_t total_proposed_resources = 0;
			for (const auto& item : request_body->proposed_resources)
				total_proposed_resources += item.second.size();
			CPPUNIT_ASSERT_EQUAL(size_t(request_body->target_num_slots * request_body->target_num_channels), total_proposed_resources);
			// For each frequency channel...
			for (const auto& item : request_body->proposed_resources) {
				const FrequencyChannel* channel = item.first;
				ReservationTable* table = reservation_manager->getReservationTable(channel);
				std::vector<unsigned int> proposed_slots;
				// ... and each slot...
				for (size_t j = 0; j < request_body->target_num_slots; j++) {
					unsigned int slot = request_body->proposed_resources[channel].at(j);
					proposed_slots.push_back(slot);
				}
				for (int offset = 0; offset < table->getPlanningHorizon(); offset++) {
					const Reservation& reservation = table->getReservation(offset);
					// ... it should be marked as RX for the proposed slots...
					if (std::find(proposed_slots.begin(), proposed_slots.end(), offset) != proposed_slots.end()) {
						CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, reservation.getAction());
						// And this channel should be saved in the last saved proposal...
						CPPUNIT_ASSERT(link_manager->lme->last_proposed_resources.find(channel) != link_manager->lme->last_proposed_resources.end());
						// ... together with this particular slot offset.
						const std::vector<unsigned int>& proposed_slots_in_this_channel = link_manager->lme->last_proposed_resources[channel];
						CPPUNIT_ASSERT(std::find(proposed_slots_in_this_channel.begin(), proposed_slots_in_this_channel.end(), offset) != proposed_slots_in_this_channel.end());
						// ... and idle for all others.
					} else
						CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, reservation.getAction());
				}
			}
			// Test it another way, too, by counting all RX reservations.
			size_t expected_num_rx_reservations = link_manager->lme->num_proposed_channels * link_manager->lme->num_proposed_slots;
			size_t actual_num_rx_reservations = 0;
			for (size_t t = 0; t < planning_horizon; t++)
				for (const auto& freq : {center_frequency1, center_frequency2, center_frequency3}) {
					ReservationTable* rx_table = reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(freq));
					if (rx_table->getReservation(t).isRx())
						actual_num_rx_reservations++;
				}
			CPPUNIT_ASSERT_EQUAL(expected_num_rx_reservations, actual_num_rx_reservations);

//                coutd.setVerbose(false);
		}

		/**
		 * Tests slot reservations after the receiver of a request has picked a candidate.
		 */
		void testReservationsAfterCandidateSelection() {
			// Send request.
			testReservationsAfterRequest();
			// Copy request proposal (otherwise we have two sides trying to delete this packet -> memory error).
			L2Packet* request_sent = phy_layer->outgoing_packets.at(0);
			L2Packet* request = link_manager->lme->prepareRequest();
			((LinkManagementEntity::ProposalPayload*) request->getPayloads().at(request->getRequestIndex()))->proposed_resources = ((LinkManagementEntity::ProposalPayload*) request_sent->getPayloads().at(request_sent->getRequestIndex()))->proposed_resources;

			// Configure a receiver side.
			TestEnvironment env2 = TestEnvironment(communication_partner_id, own_id);
			LinkManager* link_manager_rx = env2.mac_layer->getLinkManager(own_id);
			ReservationManager* reservation_manager_rx = env2.mac_layer->reservation_manager;

//                coutd.setVerbose(true);

			// Receive the request.
			CPPUNIT_ASSERT_EQUAL(size_t(0), link_manager_rx->lme->scheduled_replies.size());
			L2Packet* copy = request->copy();
			link_manager_rx->receiveFromLower(request);
			CPPUNIT_ASSERT_EQUAL(size_t(1), link_manager_rx->lme->scheduled_replies.size());

			std::vector<uint64_t> frequencies;
			frequencies.push_back(center_frequency1);
			frequencies.push_back(center_frequency2);
			frequencies.push_back(center_frequency3);
			frequencies.push_back(bc_frequency);
			auto request_payload = (LinkManagementEntity::ProposalPayload*) copy->getPayloads().at(1);
			uint64_t selected_frequency = link_manager_rx->current_channel->getCenterFrequency();
			// Go through all frequencies...
			size_t num_tx = 0, num_rx = 0;
			size_t t_tx = 0;
			for (uint64_t frequency : frequencies) {
				ReservationTable* table_rx = reservation_manager_rx->getReservationTable(
						reservation_manager_rx->getFreqChannelByCenterFreq(frequency));
				// ... for the selected frequency channel...
				if (frequency == selected_frequency) {
					for (size_t t = 0; t < table_rx->getPlanningHorizon(); t++) {
						const Reservation& reservation = table_rx->getReservation(t);
						if (reservation.isTx()) {
							num_tx++;
							t_tx = t;
							// The TX slot should be one out of the proposed slots.
							bool found_slot = false;
							for (const auto& item : request_payload->proposed_resources) {
								if (std::find(item.second.begin(), item.second.end(), t) != item.second.end())
									found_slot = true;
							}
							CPPUNIT_ASSERT_EQUAL(true, found_slot);
						} else if (reservation.isRx()) {
							num_rx++;
							// The TX slot should've been found first.
							CPPUNIT_ASSERT(t_tx > 0);
							// The RX slot should be exactly one tx_offset further than the TX slot.
							CPPUNIT_ASSERT_EQUAL(t_tx + link_manager_rx->lme->tx_offset, t);
						} else // All other slots must be idle.
							CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, reservation.getAction());
					}
					// ... for all other frequency channels...
				} else {
					for (size_t t = 0; t < table_rx->getPlanningHorizon(); t++) {
						const Reservation& reservation = table_rx->getReservation(t);
						// all slots should be IDLE.
						CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, reservation.getAction());
					}
				}
			}
			// There should be exactly one RX slot,
			CPPUNIT_ASSERT_EQUAL(size_t(1), num_rx);
			// and one TX slot.
			CPPUNIT_ASSERT_EQUAL(size_t(1), num_tx);
			// The link should still be unestablished - it updates to awaiting_data_tx only when the reply is actually sent.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, link_manager_rx->link_establishment_status);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, link_manager->link_establishment_status);
			delete copy;
//                coutd.setVerbose(false);
		}

		void testReservationsAfterReplyCameIn() {
			// Send request.
			testReservationsAfterRequest();
			// Copy request proposal (otherwise we have two sides trying to delete this packet -> memory error).
			L2Packet* request_sent = phy_layer->outgoing_packets.at(0);
			L2Packet* request = link_manager->lme->prepareRequest();
			((LinkManagementEntity::ProposalPayload*) request->getPayloads().at(request->getRequestIndex()))->proposed_resources = ((LinkManagementEntity::ProposalPayload*) request_sent->getPayloads().at(request_sent->getRequestIndex()))->proposed_resources;

			// Configure a receiver side.
			TestEnvironment env_rx = TestEnvironment(communication_partner_id, own_id);
			LinkManager* link_manager_rx = env_rx.mac_layer->getLinkManager(own_id);
			ReservationManager* reservation_manager_rx = env_rx.mac_layer->reservation_manager;

			// Receive the request, compute the reply.
			CPPUNIT_ASSERT_EQUAL(size_t(0), link_manager_rx->lme->scheduled_replies.size());
			link_manager_rx->receiveFromLower(request);
			CPPUNIT_ASSERT_EQUAL(size_t(1), link_manager_rx->lme->scheduled_replies.size());

//                coutd.setVerbose(true);

			// Make sure there are as many RX reservations as there a proposed resources.
			std::vector<uint64_t> frequencies;
			frequencies.push_back(center_frequency1);
			frequencies.push_back(center_frequency2);
			frequencies.push_back(center_frequency3);
			size_t expected_num_rx_reservations = link_manager->lme->num_proposed_channels * link_manager->lme->num_proposed_slots;
			size_t actual_num_rx_reservations = 0;
			for (size_t t = 0; t < planning_horizon; t++)
				for (const auto& freq : frequencies) {
					ReservationTable* rx_table = reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(freq));
					if (rx_table->getReservation(t).isRx())
						actual_num_rx_reservations++;
				}
			CPPUNIT_ASSERT_EQUAL(expected_num_rx_reservations, actual_num_rx_reservations);

			// Increment time until the reply has been sent.
			uint64_t selected_frequency = link_manager_rx->current_channel->getCenterFrequency();
			int reply_tx_offset = -1, first_rx_offset = -1;
			size_t num_tx = 0, num_rx = 0, num_other_reservations = 0;
			for (uint64_t frequency : frequencies) {
				ReservationTable* table_rx = reservation_manager_rx->getReservationTable(
						reservation_manager_rx->getFreqChannelByCenterFreq(frequency));
				// ... for the selected frequency channel...
				if (frequency == selected_frequency) {
					for (size_t t = 0; t < table_rx->getPlanningHorizon(); t++) {
						const Reservation& reservation = table_rx->getReservation(t);
						if (reservation.isTx() || reservation.isTxCont()) {
							num_tx++;
							reply_tx_offset = t;
						} else if (reservation.isRx()) {
							num_rx++;
							first_rx_offset = t;
						} else if (!reservation.isIdle())
							num_other_reservations++;
					}
				}
			}

			// Just one TX reserved.
			CPPUNIT_ASSERT_EQUAL(size_t(1), num_tx);
			// Just one RX reserved.
			CPPUNIT_ASSERT_EQUAL(size_t(1), num_rx);
			// No other reservations.
			CPPUNIT_ASSERT_EQUAL(size_t(0), num_other_reservations);
			// TX offset found.
			CPPUNIT_ASSERT(reply_tx_offset > -1);
			// RX offset found.
			CPPUNIT_ASSERT(first_rx_offset > -1);
			// First RX is one offset away from first TX.
			CPPUNIT_ASSERT_EQUAL(((unsigned int) reply_tx_offset) + link_manager_rx->lme->tx_offset, (unsigned int) first_rx_offset);

			std::pair<size_t, size_t> reservations;
			for (int t = 0; t < reply_tx_offset; t++) {
				mac->update(1);
				reservations = mac->execute();
			}
			// One P2P RX and one BC RX should be processed in the last time slot.
			CPPUNIT_ASSERT_EQUAL(size_t(2), reservations.second);
			// And zero TX.
			CPPUNIT_ASSERT_EQUAL(size_t(0), reservations.first);

			// Receive the reply.
			L2Packet* reply = link_manager_rx->lme->scheduled_replies.begin()->second;
			mac->receiveFromLower(reply, selected_frequency);

			// Make sure that there's no future RX reservations anymore - all should've been cleared now that we've received a reply.
			for (auto freq : frequencies) {
				ReservationTable* table = reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(freq));
				for (size_t t = 1; t < planning_horizon; t++)
					CPPUNIT_ASSERT_EQUAL(false, table->getReservation(t).isRx());
			}

			// Make sure that TX reservations are made.
			std::vector<unsigned int> tx_offsets;
			for (auto freq : frequencies) {
				ReservationTable* table = reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(freq));
				for (size_t t = 1; t < planning_horizon; t++) {
					// No reservations on any other channel...
					if (freq != selected_frequency)
						CPPUNIT_ASSERT_EQUAL(true, table->getReservation(t).isIdle());
						// ... except for the selected frequency channel, there...
					else {
						const Reservation& reservation = table->getReservation(t);
						// ... we should have some TX reservations
						if (reservation.isTx())
							tx_offsets.push_back(t);
							// ... and nothing else.
						else
							CPPUNIT_ASSERT_EQUAL(true, reservation.isIdle());
					}
				}
			}
			// As many TX reservations as a new link's timeout value.
			CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->default_tx_timeout), tx_offsets.size());
			// Timeout should be set to the default.
			CPPUNIT_ASSERT_EQUAL(link_manager->lme->default_tx_timeout, link_manager->lme->tx_timeout);
			// One TX reservation every 'tx_offset' slots.
			for (size_t i = 1; i < tx_offsets.size(); i++)
				CPPUNIT_ASSERT_EQUAL(tx_offsets.at(i - 1) + link_manager->lme->tx_offset, tx_offsets.at(i));
			// First TX reservation after one 'tx_offset".
			CPPUNIT_ASSERT_EQUAL(link_manager->lme->tx_offset, tx_offsets.at(0));

			// Make sure request slots are marked.
			CPPUNIT_ASSERT(link_manager->lme->max_link_renewal_attempts > 0);
			CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->max_link_renewal_attempts), link_manager->lme->scheduled_requests.size());
			uint64_t expiry_offset = tx_offsets.at(0) + link_manager->lme->tx_offset * link_manager->lme->tx_timeout;
			uint64_t current_absolute_slot = mac->getCurrentSlot();
			for (uint64_t request_slot : link_manager->lme->scheduled_requests) {
				CPPUNIT_ASSERT(request_slot < expiry_offset);
				CPPUNIT_ASSERT_EQUAL(true, std::any_of(tx_offsets.begin(), tx_offsets.end(), [request_slot, current_absolute_slot](uint64_t tx_slot) {
					// `request_slot` are absolute slots, so subtracting the current absolute slot transforms to an offset,
					// which makes it comparable to `tx_slot`, which is also an offset.
					return tx_slot == (request_slot - current_absolute_slot);
				}));
			}

			// The link should now be established.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, link_manager->link_establishment_status);

//                coutd.setVerbose(false);
		}

		void testReservationsAfterFirstDataTx() {
			TestEnvironment env_rx = TestEnvironment(communication_partner_id, own_id);
			LinkManager* link_manager_rx = env_rx.mac_layer->getLinkManager(own_id);

			// Send request.
			testReservationsAfterRequest();
			// Copy request proposal (otherwise we have two sides trying to delete this packet -> memory error).
			L2Packet* request_sent = phy_layer->outgoing_packets.at(0);
			L2Packet* request = link_manager->lme->prepareRequest();
			((LinkManagementEntity::ProposalPayload*) request->getPayloads().at(request->getRequestIndex()))->proposed_resources = ((LinkManagementEntity::ProposalPayload*) request_sent->getPayloads().at(request_sent->getRequestIndex()))->proposed_resources;
			// Receive the request.
			link_manager_rx->receiveFromLower(request);
			// Increment time until the reply has been sent.
			size_t num_slots = 0, max_num_slots = 20;
			while (env_rx.phy_layer->outgoing_packets.empty() && num_slots++ < max_num_slots) {
				env_rx.mac_layer->update(1);
				env_rx.mac_layer->execute();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);

			// Receive the reply.
			CPPUNIT_ASSERT_EQUAL(size_t(1), env_rx.phy_layer->outgoing_packets.size());
			L2Packet* reply_sent = env_rx.phy_layer->outgoing_packets.at(0);
			// Copy the content to avoid double-free memory error.
			L2Packet* reply = link_manager_rx->lme->prepareReply(own_id);
			((LinkManagementEntity::ProposalPayload*) reply->getPayloads().at(1))->proposed_resources = ((LinkManagementEntity::ProposalPayload*) reply_sent->getPayloads().at(1))->proposed_resources;
			// Receive the reply.
			uint64_t selected_freq = env_rx.phy_layer->outgoing_packet_freqs.at(0);
			phy_layer->tuneReceiver(selected_freq);
			phy_layer->onReception(reply, selected_freq);

//                coutd.setVerbose(true);

			// Should've only sent the request so far.
			CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());
			// Increment time until the first data transmission.
			uint64_t slots_until_tx = link_manager->lme->tx_offset;
			mac->update(slots_until_tx);
			env_rx.mac_layer->update(slots_until_tx);
			mac->execute();
			// Should have the first transmission "sent" now.
			CPPUNIT_ASSERT_EQUAL(size_t(2), phy_layer->outgoing_packets.size());
			// Let RX receive it.
			auto* data_packet = phy_layer->outgoing_packets.at(1)->copy();
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_data_tx, link_manager_rx->link_establishment_status);
			link_manager_rx->receiveFromLower(data_packet);

			// It should now have an established link.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, link_manager_rx->link_establishment_status);
			// Both sides should have matching (TX, RX)-pairs of reservations.
			ReservationTable* table_tx = link_manager->current_reservation_table, * table_rx = link_manager_rx->current_reservation_table;
			size_t expected_num_reservations = link_manager->lme->default_tx_timeout, actual_num_reservations = 0;
			std::vector<size_t> expected_offsets;
			for (size_t t = 0; t < expected_num_reservations; t++)
				expected_offsets.push_back(t * link_manager->lme->tx_offset);
			for (size_t t = 0; t < planning_horizon; t++) {
				const Reservation& res_tx = table_tx->getReservation(t), &res_rx = table_rx->getReservation(t);
				if (res_tx.isTx()) {
					actual_num_reservations++;
					CPPUNIT_ASSERT_EQUAL(communication_partner_id, res_tx.getTarget());
					CPPUNIT_ASSERT_EQUAL(own_id, res_rx.getTarget());
					CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, res_rx.getAction());
					CPPUNIT_ASSERT_EQUAL(true, std::any_of(expected_offsets.begin(), expected_offsets.end(), [t](size_t offset) {
						return offset == t;
					}));
				} else {
					CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, res_tx.getAction());
					CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, res_rx.getAction());
					CPPUNIT_ASSERT_EQUAL(SYMBOLIC_ID_UNSET, res_tx.getTarget());
					CPPUNIT_ASSERT_EQUAL(SYMBOLIC_ID_UNSET, res_rx.getTarget());
				}
			}
			CPPUNIT_ASSERT_EQUAL(expected_num_reservations, actual_num_reservations);

			CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->max_link_renewal_attempts), link_manager->lme->scheduled_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), link_manager_rx->lme->scheduled_requests.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), link_manager_rx->lme->scheduled_replies.size());
			CPPUNIT_ASSERT_EQUAL(size_t(0), link_manager->lme->scheduled_replies.size());

//                coutd.setVerbose(false);
		}

		/**
		 * Ensures that the local timeout counter on the TX side decreases with the number of transmissions made.
		 */
		void testLinkExpiry() {
			CPPUNIT_ASSERT_EQUAL(link_manager->lme->tx_timeout, link_manager->lme->default_tx_timeout);
			testReservationsAfterFirstDataTx();
			// No renewal attempts are made if there's no more data.
			rlc_layer->should_there_be_more_data = false;
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established, link_manager->link_establishment_status);
			CPPUNIT_ASSERT(link_manager->lme->default_tx_timeout > 0);
			unsigned int current_timeout = link_manager->lme->default_tx_timeout - 1;
			CPPUNIT_ASSERT_EQUAL(current_timeout, link_manager->lme->tx_timeout);

//			coutd.setVerbose(true);

			unsigned int final_slot = current_timeout * link_manager->lme->tx_offset;
			// Have the link expire.
			for (unsigned int t = 0; t < final_slot; t += link_manager->lme->tx_offset) {
				mac->update(link_manager->lme->tx_offset);
				mac->execute();
				current_timeout--;
				CPPUNIT_ASSERT_EQUAL(current_timeout, link_manager->lme->tx_timeout);
			}
			// Should now be "not established" again.
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, link_manager->link_establishment_status);
//			coutd.setVerbose(false);
		}

		/**
		 * Makes sure that requests are sent at every scheduled request slot.
		 */
		void testLinkRenewalRequest() {
			testReservationsAfterFirstDataTx();
			// Renewal attempts *are* made if there's more data.
			rlc_layer->should_there_be_more_data = true;

			// 1st request + 1 data packet should've been sent so far.
			size_t expected_num_sent_packets = 2;
			CPPUNIT_ASSERT_EQUAL(expected_num_sent_packets, phy_layer->outgoing_packets.size());

//			coutd.setVerbose(true);
			size_t num_slots = 0, max_slots = 1000;
			// Increment time to each request slot...
			while (num_slots++ < max_slots && !link_manager->lme->scheduled_requests.empty()) {
				uint64_t request_slot = *std::min_element(link_manager->lme->scheduled_requests.begin(), link_manager->lme->scheduled_requests.end());
				mac->update(request_slot - mac->getCurrentSlot());
				mac->execute();
				expected_num_sent_packets++;
				// ... make sure a new request has been sent
				CPPUNIT_ASSERT_EQUAL(expected_num_sent_packets, phy_layer->outgoing_packets.size());
				L2Packet* request = phy_layer->outgoing_packets.at(phy_layer->outgoing_packets.size() - 1);
				CPPUNIT_ASSERT_EQUAL(size_t(2), request->getHeaders().size());
				CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request,request->getHeaders().at(1)->frame_type);
				// Current slot should be used to transmit the request.
				CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, link_manager->current_reservation_table->getReservation(0).getAction());
				// And next burst to receive the reply.
				CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, link_manager->current_reservation_table->getReservation(link_manager->lme->tx_offset).getAction());
			}

			CPPUNIT_ASSERT(num_slots < max_slots);
			CPPUNIT_ASSERT_EQUAL(true, link_manager->lme->scheduled_requests.empty());

//			coutd.setVerbose(false);
		}

		void testLinkRenewalReply() {
			TestEnvironment env_rx = TestEnvironment(communication_partner_id, own_id);
			env_rx.phy_layer->connected_phy = phy_layer;
			phy_layer->connected_phy = env_rx.phy_layer;
			auto* mac_rx = env_rx.mac_layer;

			rlc_layer->should_there_be_more_data = true;
			arq_layer->should_forward = true;
			// Trigger link establishment.
			CPPUNIT_ASSERT_EQUAL(size_t(0), rlc_layer->control_message_injections.size());
			mac->notifyOutgoing(1024, communication_partner_id);

//			coutd.setVerbose(true);
			// Increment time until link is established.
			size_t num_slots = 0, max_num_slots = 1000;
			while (link_manager->link_establishment_status != LinkManager::link_established && num_slots++ < max_num_slots) {
				mac->update(1);
				mac_rx->update(1);
				mac->execute();
				mac_rx->execute();
			}
			CPPUNIT_ASSERT(num_slots < max_num_slots);
			CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_established,link_manager->link_establishment_status);

			// Increment time until link request has been sent.
			size_t num_scheduled_requests = link_manager->lme->max_link_renewal_attempts;
			CPPUNIT_ASSERT_EQUAL(num_scheduled_requests, link_manager->lme->scheduled_requests.size());
			num_slots = 0;
			while (link_manager->lme->scheduled_requests.size() != num_scheduled_requests - 1 && num_slots++ < max_num_slots) {
				mac->update(1);
				mac_rx->update(1);
				mac->execute();
				mac_rx->execute();
			}
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, link_manager->current_reservation_table->getReservation(0).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, link_manager->current_reservation_table->getReservation(link_manager->lme->tx_offset).getAction());
			LinkManager* link_manager_rx = mac_rx->getLinkManager(own_id);
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, link_manager_rx->current_reservation_table->getReservation(0).getAction());
			CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, link_manager_rx->current_reservation_table->getReservation(link_manager->lme->tx_offset).getAction());
//			coutd.setVerbose(false);
		}


	CPPUNIT_TEST_SUITE(LinkManagerTests);
			CPPUNIT_TEST(testTrafficEstimate);
			CPPUNIT_TEST(testTrafficEstimateOverTimeslots);
			CPPUNIT_TEST(testNewLinkEstablishment);
			CPPUNIT_TEST(testComputeProposal);
			CPPUNIT_TEST(testTransmissionSlotOnUnestablishedLink);
			CPPUNIT_TEST(testNewLinkRequest);
			CPPUNIT_TEST(testOnTransmissionSlot);
			CPPUNIT_TEST(testSetBaseHeader);
			CPPUNIT_TEST(testSetBeaconHeader);
			CPPUNIT_TEST(testSetUnicastHeader);
			CPPUNIT_TEST(testSetRequestHeader);
			CPPUNIT_TEST(testProcessIncomingBase);
			CPPUNIT_TEST(testProcessIncomingLinkEstablishmentRequest);
			CPPUNIT_TEST(testProcessIncomingUnicast);
			CPPUNIT_TEST(testPrepareLinkEstablishmentRequest);
			CPPUNIT_TEST(testPrepareLinkReply);
			CPPUNIT_TEST(testReplyToRequest);
			CPPUNIT_TEST(testLocking);
			CPPUNIT_TEST(testReservationsAfterRequest);
			CPPUNIT_TEST(testReservationsAfterCandidateSelection);
			CPPUNIT_TEST(testReservationsAfterReplyCameIn);
			CPPUNIT_TEST(testReservationsAfterFirstDataTx);
			CPPUNIT_TEST(testLinkExpiry);
			CPPUNIT_TEST(testLinkRenewalRequest);
//			CPPUNIT_TEST(testLinkRenewalReply);
		CPPUNIT_TEST_SUITE_END();
	};
}