//
// Created by Sebastian Lindner on 18.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../LinkManager.hpp"
#include "../BCLinkManager.hpp"
#include "../LinkManagementEntity.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

    /**
     * The LinkManager is the core component of the IntAirNet' LDACS MAC.
     * These tests aim at one side of the communication link, e.g. the preparation of a request and testing its contents.
     * Tests that involve both TX *and* RX are put into SystemTests instead.
     */
	class LinkManagerTests : public CppUnit::TestFixture {
		private:
			LinkManager* link_manager;
			ReservationManager* reservation_manager;
			MacId own_id = MacId(42);
			MacId communication_partner_id = MacId(43);
			uint32_t planning_horizon = 128;
			uint64_t center_frequency1 = 962, center_frequency2 = 963, center_frequency3 = 964, bc_frequency = 965, bandwidth = 500;
			unsigned long num_bits_going_out = 800*100;
			MACLayer* mac;
			ARQLayer* arq_layer;
			RLCLayer* rlc_layer;
			PHYLayer* phy_layer;
			NetworkLayer* net_layer;
		
		public:
			void setUp() override {
				phy_layer = new PHYLayer(planning_horizon);
				mac = new MACLayer(own_id, planning_horizon);
				reservation_manager = mac->reservation_manager;
				reservation_manager->setPhyTransmitterTable(phy_layer->getTransmitterReservationTable());
				reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				
				link_manager = mac->getLinkManager(communication_partner_id);
				arq_layer = new ARQLayer();
				mac->setUpperLayer(arq_layer);
				arq_layer->setLowerLayer(mac);
				net_layer = new NetworkLayer();
				rlc_layer = new RLCLayer(own_id);
				net_layer->setLowerLayer(rlc_layer);
				rlc_layer->setUpperLayer(net_layer);
				rlc_layer->setLowerLayer(arq_layer);
				arq_layer->setUpperLayer(rlc_layer);
				phy_layer->setUpperLayer(mac);
				mac->setLowerLayer(phy_layer);
			}
			
			void tearDown() override {
				delete mac;
				delete arq_layer;
				delete rlc_layer;
				delete phy_layer;
				delete net_layer;
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
					CPPUNIT_ASSERT_EQUAL(sum / (i+1), link_manager->getCurrentTrafficEstimate());
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
			    // ... and then shrink it per slot by not notying about any new data
			    for (size_t i = 0; i < link_manager->traffic_estimate.values.size(); i++) {
			        mac->update(1);
			        expected_estimate -= ((double) bits_to_send /  ((double) link_manager->traffic_estimate.values.size()));
			        // Comparing doubles...
                    CPPUNIT_ASSERT(link_manager->getCurrentTrafficEstimate() > expected_estimate - 1 && link_manager->getCurrentTrafficEstimate() < expected_estimate + 1);
			    }
			}
			
			void testNewLinkEstablishment() {
				// It must a be a P2P link.
				CPPUNIT_ASSERT(own_id != SYMBOLIC_LINK_ID_BROADCAST && own_id != SYMBOLIC_LINK_ID_BEACON);
				// Initially the link should not be established.
				CPPUNIT_ASSERT_EQUAL(LinkManager::Status::link_not_established, link_manager->link_establishment_status);
				CPPUNIT_ASSERT_EQUAL(size_t(0), rlc_layer->injections.size());
				// Now inform the LinkManager of new data for this link.
				link_manager->notifyOutgoing(num_bits_going_out);
				// The RLC should've received a link request.
				CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer->injections.size());
				CPPUNIT_ASSERT_EQUAL(size_t(2), rlc_layer->injections.at(0)->getHeaders().size());
				CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::base, rlc_layer->injections.at(0)->getHeaders().at(0)->frame_type);
				CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, rlc_layer->injections.at(0)->getHeaders().at(1)->frame_type);
				// And the LinkManager status should've updated.
				CPPUNIT_ASSERT_EQUAL(LinkManager::Status::awaiting_reply, link_manager->link_establishment_status);
			}
			
			void testComputeProposal() {
				testNewLinkEstablishment();
				L2Packet* request = rlc_layer->injections.at(0);
                LinkManagementEntity::ProposalPayload* proposal = link_manager->lme->p2pSlotSelection();
				CPPUNIT_ASSERT_EQUAL(size_t(2), request->getPayloads().size());
				
				// Should've considered several distinct frequency channels.
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->num_proposed_channels), proposal->proposed_channels.size());
				for (size_t i = 1; i < proposal->proposed_channels.size(); i++) {
					const FrequencyChannel* channel0 = proposal->proposed_channels.at(i-1);
					const FrequencyChannel* channel1 = proposal->proposed_channels.at(i);
					CPPUNIT_ASSERT(channel1->getCenterFrequency() != channel0->getCenterFrequency());
				}
				
				// Should've considered a number of candidate slots per frequency channel.
				for (auto num_slots_in_this_candidate : proposal->num_candidates) {
					// Since all are idle, we should've found the target number each time.
					CPPUNIT_ASSERT_EQUAL(link_manager->lme->num_proposed_slots, num_slots_in_this_candidate);
				}
				// and so the grand total should be the number of proposed slots times the number of proposed channels.
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->num_proposed_channels * link_manager->lme->num_proposed_slots), proposal->proposed_slots.size());
				
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
				CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer->injections.size());
				L2Packet* request = rlc_layer->injections.at(0);
				CPPUNIT_ASSERT_EQUAL(size_t(2), request->getHeaders().size());
				CPPUNIT_ASSERT(request->getHeaders().at(0)->frame_type == L2Header::base);
				CPPUNIT_ASSERT(request->getHeaders().at(1)->frame_type == L2Header::link_establishment_request);
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
				CPPUNIT_ASSERT_EQUAL(own_id, header.icao_id);
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
				ReservationTable* table = reservation_manager->reservation_tables.at(0);
				link_manager->current_reservation_table = table;
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
						const Reservation& cont_reservation = table->getReservation((i+1) * offset + j);
						CPPUNIT_ASSERT_EQUAL(communication_partner_id, cont_reservation.getTarget());
						CPPUNIT_ASSERT_EQUAL(true,cont_reservation.isRx());
						CPPUNIT_ASSERT_EQUAL(length_next - 1 - j, cont_reservation.getNumRemainingSlots());
					}
					
				}
			}
			
			void testProcessIncomingLinkEstablishmentRequest() {
				// Assign a reservation table.
				ReservationTable* table = reservation_manager->reservation_tables.at(0);
				link_manager->current_reservation_table = table;
				// Prepare a link establishment request by our communication partner.
				MACLayer other_mac = MACLayer(communication_partner_id, planning_horizon);
				other_mac.setUpperLayer(arq_layer);
				other_mac.setLowerLayer(phy_layer);
				other_mac.reservation_manager->setPhyTransmitterTable(phy_layer->getTransmitterReservationTable());
				other_mac.reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				other_mac.reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				other_mac.reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				other_mac.reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				LinkManager other_link_manager = LinkManager(own_id, other_mac.reservation_manager, &other_mac);
//				coutd.setVerbose(true);
				L2Packet* request = other_link_manager.lme->prepareRequest();
				request->getPayloads().at(1) = other_link_manager.lme->p2pSlotSelection();
				// The number of proposed channels should be adequate.
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->lme->num_proposed_channels), ((LinkManagementEntity::ProposalPayload*) request->getPayloads().at(1))->proposed_channels.size());
				auto header = (L2HeaderLinkEstablishmentRequest*) request->getHeaders().at(1);
				auto body = (LinkManagementEntity::ProposalPayload*) request->getPayloads().at(1);
				auto viable_candidates = link_manager->lme->findViableCandidatesInRequest(header, body);
				// And all slots should be viable.
				CPPUNIT_ASSERT_EQUAL((size_t) link_manager->lme->num_proposed_channels * link_manager->lme->num_proposed_slots, viable_candidates.size());
//				coutd.setVerbose(false);
			}
			
			void testProcessIncomingUnicast() {
				// Assign a reservation table.
				ReservationTable* table = reservation_manager->reservation_tables.at(0);
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
				MACLayer other_mac = MACLayer(communication_partner_id, planning_horizon);
				other_mac.setUpperLayer(arq_layer);
				other_mac.setLowerLayer(phy_layer);
				other_mac.reservation_manager->setPhyTransmitterTable(phy_layer->getTransmitterReservationTable());
				other_mac.reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				other_mac.reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				other_mac.reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				other_mac.reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				LinkManager other_link_manager = LinkManager(own_id, other_mac.reservation_manager, &other_mac);
				L2Packet* request = other_link_manager.lme->prepareRequest();
				request->getPayloads().at(1) = other_link_manager.lme->p2pSlotSelection();
//				coutd.setVerbose(true);
				// Receive it on the BC.
				bc_manager->receiveFromLower(request);
				// Fetch the now-instantiated P2P manager.
				LinkManager* p2p_manager = mac->getLinkManager(communication_partner_id);
				// And increment time until it has sent the reply.
				CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
				while (!p2p_manager->lme->scheduled_replies.empty()) {
					mac->update(1);
					mac->execute();
				}
				CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());
				L2Packet* reply = phy_layer->outgoing_packets.at(0);
				CPPUNIT_ASSERT_EQUAL(own_id, reply->getOrigin());
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reply->getDestination());
				// Link establishment status should've been updated.
				CPPUNIT_ASSERT_EQUAL(LinkManager::Status::reply_sent, p2p_manager->link_establishment_status);
//				coutd.setVerbose(false);
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
				CPPUNIT_ASSERT_EQUAL(proposal1->proposed_channels.size(), proposal2->proposed_channels.size());
				for (size_t i = 0; i < proposal1->proposed_channels.size(); i++)
					CPPUNIT_ASSERT_EQUAL(proposal1->proposed_channels.at(i), proposal2->proposed_channels.at(i));
				// But the slots mustn't be the same.
				for (int32_t slot1 : proposal1->proposed_slots) { // Any out of the first proposed slots...
					for (int32_t slot2 : proposal2->proposed_slots) // can't equal any of the others.
						CPPUNIT_ASSERT(slot1 != slot2);
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
			    CPPUNIT_ASSERT_EQUAL(size_t(0), rlc_layer->injections.size());
			    mac->notifyOutgoing(1024, communication_partner_id);
			    // Request should've been injected.
                CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer->injections.size());
                CPPUNIT_ASSERT_EQUAL(L2Header::FrameType::link_establishment_request, rlc_layer->injections.at(0)->getHeaders().at(1)->frame_type);
                // Broadcast LinkManager should've been notified.
                CPPUNIT_ASSERT_EQUAL(true, bc_link_manager->broadcast_slot_scheduled);

                // Increment time until the reply has been sent.
                CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
                size_t num_slots = 0, max_num_slots = 10;
                while (bc_link_manager->broadcast_slot_scheduled && num_slots < max_num_slots) {
                    mac->update(1);
                    mac->execute();
                    num_slots++;
                }
                CPPUNIT_ASSERT(num_slots < max_num_slots);
                CPPUNIT_ASSERT_EQUAL(false, bc_link_manager->broadcast_slot_scheduled);

                // Request should've been sent.
                CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());

                // Now RX reservations should've been made at all proposed slots.
                L2Packet* request = phy_layer->outgoing_packets.at(0);
                auto* request_header = (L2HeaderLinkEstablishmentRequest*) request->getHeaders().at(1);
                auto* request_body = (LinkManagementEntity::ProposalPayload*) request->getPayloads().at(1);
                CPPUNIT_ASSERT_EQUAL(size_t(request_body->target_num_channels), request_body->proposed_channels.size());
                CPPUNIT_ASSERT_EQUAL(size_t(request_body->target_num_slots * request_body->target_num_channels), request_body->proposed_slots.size());
                // For each frequency channel...
                for (size_t i = 0; i < request_body->proposed_channels.size(); i++) {
                    const FrequencyChannel* channel = request_body->proposed_channels.at(i);
                    ReservationTable* table = reservation_manager->getReservationTable(channel);
                    std::vector<unsigned int> proposed_slots;
                    // ... and each slot...
                    for (size_t j = 0; j < request_body->target_num_slots; j++) {
                        unsigned int slot = request_body->proposed_slots.at(i*request_body->target_num_slots + j);
                        proposed_slots.push_back(slot);
                    }
                    for (unsigned int offset = 0; offset < table->getPlanningHorizon(); offset++) {
                        const Reservation& reservation = table->getReservation(offset);
                        // ... it should be marked as RX for the proposed slots...
                        if (std::find(proposed_slots.begin(), proposed_slots.end(), offset) != proposed_slots.end())
                            CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, reservation.getAction());
                        // ... and idle for all others.
                        else
                            CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, reservation.getAction());
                    }
                }

//                coutd.setVerbose(false);
			}

			/**
			 * Tests slot reservations after the receiver of a request has picked a candidate.
			 */
			void testReservationsAfterCandidateSelection() {
			    // Prepare a request.
                L2Packet* request = link_manager->lme->prepareRequest();
                link_manager->lme->populateRequest(request);

                // Configure a receiver side.
                PHYLayer phy_layer_rx = PHYLayer(planning_horizon);
                MACLayer mac_rx = MACLayer(communication_partner_id, planning_horizon);
                ReservationManager* reservation_manager_rx = mac_rx.reservation_manager;
                reservation_manager_rx->setPhyTransmitterTable(phy_layer->getTransmitterReservationTable());
                reservation_manager_rx->addFrequencyChannel(false, bc_frequency, bandwidth);
                reservation_manager_rx->addFrequencyChannel(true, center_frequency1, bandwidth);
                reservation_manager_rx->addFrequencyChannel(true, center_frequency2, bandwidth);
                reservation_manager_rx->addFrequencyChannel(true, center_frequency3, bandwidth);
                LinkManager* link_manager_rx = mac_rx.getLinkManager(own_id);
                ARQLayer arq_layer_rx = ARQLayer();
                mac_rx.setUpperLayer(&arq_layer_rx);
                arq_layer_rx.setLowerLayer(&mac_rx);
                NetworkLayer net_layer_rx = NetworkLayer();
                RLCLayer rlc_layer_rx = RLCLayer(communication_partner_id);
                net_layer_rx.setLowerLayer(&rlc_layer_rx);
                rlc_layer_rx.setUpperLayer(&net_layer_rx);
                rlc_layer_rx.setLowerLayer(&arq_layer_rx);
                arq_layer_rx.setUpperLayer(&rlc_layer_rx);
                phy_layer_rx.setUpperLayer(&mac_rx);
                mac_rx.setLowerLayer(&phy_layer_rx);

//                coutd.setVerbose(true);

                // Receive the request.
                CPPUNIT_ASSERT_EQUAL(size_t(0), link_manager_rx->lme->scheduled_replies.size());
                link_manager_rx->receiveFromLower(request);
                CPPUNIT_ASSERT_EQUAL(size_t(1), link_manager_rx->lme->scheduled_replies.size());

                std::vector<uint64_t> frequencies;
                frequencies.push_back(center_frequency1);
                frequencies.push_back(center_frequency2);
                frequencies.push_back(center_frequency3);
                frequencies.push_back(bc_frequency);
                auto request_payload = (LinkManagementEntity::ProposalPayload*) request->getPayloads().at(1);
                uint64_t selected_frequency = link_manager_rx->current_channel->getCenterFrequency();
                // Go through all frequencies...
                size_t num_tx = 0, num_rx = 0;
                size_t t_tx = 0;
                for (uint64_t frequency : frequencies) {
                    ReservationTable *table_rx = reservation_manager_rx->getReservationTable(
                            reservation_manager_rx->getFreqChannelByCenterFreq(frequency));
                    // ... for the selected frequency channel...
                    if (frequency == selected_frequency) {
                        for (size_t t = 0; t < table_rx->getPlanningHorizon(); t++) {
                            const Reservation &reservation = table_rx->getReservation(t);
                            if (reservation.isTx()) {
                                num_tx++;
                                t_tx = t;
                                // The TX slot should be one out of the proposed slots.
                                CPPUNIT_ASSERT(std::find(request_payload->proposed_slots.begin(),
                                                         request_payload->proposed_slots.end(), t) !=
                                               request_payload->proposed_slots.end());
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
                            const Reservation &reservation = table_rx->getReservation(t);
                            // all slots should be IDLE.
                            CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, reservation.getAction());
                        }
                    }
                }
                // There should be exactly one RX slot,
                CPPUNIT_ASSERT_EQUAL(size_t(1), num_rx);
                // and one TX slot.
                CPPUNIT_ASSERT_EQUAL(size_t(1), num_tx);

//                coutd.setVerbose(false);
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
		CPPUNIT_TEST_SUITE_END();
	};
}