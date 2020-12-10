//
// Created by Sebastian Lindner on 18.11.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../LinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class LinkManagerTests : public CppUnit::TestFixture {
		private:
			LinkManager* link_manager;
			ReservationManager* reservation_manager;
			MacId own_id = MacId(42);
			MacId communication_partner_id = MacId(43);
			uint32_t planning_horizon = 128;
			uint64_t center_frequency1 = 1000, center_frequency2 = 2000, center_frequency3 = 3000, bc_frequency = 4000, bandwidth = 500;
			unsigned long num_bits_going_out = 800*100;
			MACLayer* mac;
			ARQLayer* arq_layer;
			RLCLayer* rlc_layer;
			PHYLayer* phy_layer;
			NetworkLayer* net_layer;
		
		public:
			void setUp() override {
				phy_layer = new PHYLayer(planning_horizon);
				reservation_manager = new ReservationManager(planning_horizon);
				reservation_manager->setPhyTransmitterTable(phy_layer->getTransmitterReservationTable());
				reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
				reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);
				mac = new MACLayer(own_id, reservation_manager);
				link_manager = new LinkManager(communication_partner_id, reservation_manager, mac);
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
				delete reservation_manager;
				delete mac;
				delete link_manager;
				delete arq_layer;
				delete rlc_layer;
				delete phy_layer;
				delete net_layer;
			}
			
			void testTrafficEstimate() {
				CPPUNIT_ASSERT_EQUAL(0.0, link_manager->getCurrentTrafficEstimate());
				unsigned int initial_bits = 10;
				unsigned int num_bits = initial_bits;
				double sum = 0;
				// Fill up the window.
				for (size_t i = 0; i < link_manager->getTrafficEstimateWindowSize(); i++) {
					link_manager->updateTrafficEstimate(num_bits);
					sum += num_bits;
					num_bits += initial_bits;
					CPPUNIT_ASSERT_EQUAL(sum / (i+1), link_manager->getCurrentTrafficEstimate());
				}
				// Now it's full, so the next input will kick out the first value.
				link_manager->updateTrafficEstimate(num_bits);
				sum -= initial_bits;
				sum += num_bits;
				CPPUNIT_ASSERT_EQUAL(sum / (link_manager->getTrafficEstimateWindowSize()), link_manager->getCurrentTrafficEstimate());
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
				LinkManager::ProposalPayload* proposal = link_manager->computeRequestProposal();
				CPPUNIT_ASSERT_EQUAL(size_t(2), request->getPayloads().size());
				
				// Should've considered several distinct frequency channels.
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->num_proposed_channels), proposal->proposed_channels.size());
				for (size_t i = 1; i < proposal->proposed_channels.size(); i++) {
					const FrequencyChannel* channel0 = proposal->proposed_channels.at(i-1);
					const FrequencyChannel* channel1 = proposal->proposed_channels.at(i);
					CPPUNIT_ASSERT(channel1->getCenterFrequency() != channel0->getCenterFrequency());
				}
				
				// Should've considered a number of candidate slots per frequency channel.
				for (auto num_slots_in_this_candidate : proposal->num_candidates) {
					// Since all are idle, we should've found the target number each time.
					CPPUNIT_ASSERT_EQUAL(link_manager->num_proposed_slots, num_slots_in_this_candidate);
				}
				// and so the grand total should be the number of proposed slots times the number of proposed channels.
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->num_proposed_channels * link_manager->num_proposed_slots), proposal->proposed_slots.size());
				
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
				link_manager->requestNewLink();
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
					link_manager->onTransmissionSlot(1);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
			}
			
			void testOnTransmissionSlot() {
				CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
				// Transmission slots should only occur for established links.
				link_manager->link_establishment_status = LinkManager::link_established;
				L2Packet* packet = link_manager->onTransmissionSlot(1);
				CPPUNIT_ASSERT(packet != nullptr);
				delete packet;
			}
			
			void testTriggerNewLinkRequest() {
				// Next transmission should trigger a new request.
				// Transmission slots should only occur for established links.
				link_manager->link_establishment_status = LinkManager::link_established;
				link_manager->TIMEOUT_THRESHOLD_TRIGGER = 3;
				link_manager->current_reservation_timeout = link_manager->TIMEOUT_THRESHOLD_TRIGGER + 1;
				link_manager->onTransmissionSlot(1);
				CPPUNIT_ASSERT_EQUAL(size_t(1), rlc_layer->injections.size());
				L2Packet* request = rlc_layer->injections.at(0);
				CPPUNIT_ASSERT_EQUAL(size_t(2), request->getHeaders().size());
				CPPUNIT_ASSERT(request->getHeaders().at(0)->frame_type == L2Header::base);
				CPPUNIT_ASSERT(request->getHeaders().at(1)->frame_type == L2Header::link_establishment_request);
			}
			
			void testSetBaseHeader() {
				L2HeaderBase header = L2HeaderBase();
				link_manager->setHeaderFields(&header);
				CPPUNIT_ASSERT_EQUAL(own_id, header.icao_id);
				CPPUNIT_ASSERT_EQUAL(link_manager->current_reservation_offset, header.offset);
				CPPUNIT_ASSERT_EQUAL(link_manager->current_reservation_slot_length, header.length_next);
				CPPUNIT_ASSERT_EQUAL(link_manager->current_reservation_timeout + 1, header.timeout);
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
				L2Packet packet = L2Packet();
				unsigned int offset = 5, length_next = 2, timeout = 3;
				auto* base_header = new L2HeaderBase(communication_partner_id, offset, length_next, timeout);
				packet.addPayload(base_header, nullptr);
				// Have the LinkManager process it.
				link_manager->receiveFromLower(&packet);
				// Ensure that the slots were marked.
				for (size_t i = 0; i < timeout; i++) {
					const Reservation& reservation = table->getReservation((i + 1) * offset);
					CPPUNIT_ASSERT_EQUAL(communication_partner_id, reservation.getOwner());
					CPPUNIT_ASSERT_EQUAL(Reservation::TX, reservation.getAction());
					CPPUNIT_ASSERT_EQUAL(length_next - 1, reservation.getNumRemainingTxSlots());
					for (unsigned int j = 1; j < length_next; j++) {
						const Reservation& cont_reservation = table->getReservation((i+1) * offset + j);
						CPPUNIT_ASSERT_EQUAL(communication_partner_id, cont_reservation.getOwner());
						CPPUNIT_ASSERT_EQUAL(Reservation::TX_CONT,cont_reservation.getAction());
						CPPUNIT_ASSERT_EQUAL(length_next - 1 - j, cont_reservation.getNumRemainingTxSlots());
					}
					
				}
			}
			
			void testProcessIncomingLinkEstablishmentRequest() {
				// Assign a reservation table.
				ReservationTable* table = reservation_manager->reservation_tables.at(0);
				link_manager->current_reservation_table = table;
				// Prepare a link establishment request.
//				coutd.setVerbose(true);
				L2Packet* request = link_manager->prepareLinkEstablishmentRequest();
				request->getPayloads().at(1) = link_manager->computeRequestProposal();
				CPPUNIT_ASSERT_EQUAL(size_t(link_manager->num_proposed_channels), ((LinkManager::ProposalPayload*) request->getPayloads().at(1))->proposed_channels.size());
				auto header = (L2HeaderLinkEstablishmentRequest*) request->getHeaders().at(1);
				auto body = (LinkManager::ProposalPayload*) request->getPayloads().at(1);
				auto viable_candidates = link_manager->processIncomingLinkEstablishmentRequest(header, body);
				CPPUNIT_ASSERT_EQUAL((size_t) link_manager->num_proposed_channels * link_manager->num_proposed_slots, viable_candidates.size());
//				coutd.setVerbose(false);
			}
			
			void testProcessIncomingUnicast() {
				// Assign a reservation table.
				ReservationTable* table = reservation_manager->reservation_tables.at(0);
				link_manager->current_reservation_table = table;
				// When we receive a packet intended for us...
				L2Packet* unicast_packet_intended_for_us = rlc_layer->requestSegment(phy_layer->getCurrentDatarate(), own_id);
				auto* header_for_us = (L2HeaderUnicast*) unicast_packet_intended_for_us->getHeaders().at(1);
				L2Packet::Payload* payload_for_us = unicast_packet_intended_for_us->getPayloads().at(1);
				CPPUNIT_ASSERT(header_for_us != nullptr);
				CPPUNIT_ASSERT(payload_for_us != nullptr);
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
				L2Packet* request = link_manager->prepareLinkEstablishmentRequest();
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, request->getDestination());
				CPPUNIT_ASSERT_EQUAL(own_id, request->getOrigin());
			}
			
			void testPrepareLinkReply() {
				L2Packet* reply = link_manager->prepareLinkEstablishmentReply(communication_partner_id);
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reply->getDestination());
				CPPUNIT_ASSERT_EQUAL(own_id, reply->getOrigin());
			}
			
			void testReplyToRequest() {
				// Assign as BC link manager.
				LinkManager* bc_manager = mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
				// Prepare a link establishment request.
//				coutd.setVerbose(true);
				L2Packet* request = bc_manager->prepareLinkEstablishmentRequest();
				request->getPayloads().at(1) = bc_manager->computeRequestProposal();
				// Hackily set it as if this request came from our communication partner.
				((L2HeaderBase*) request->getHeaders().at(0))->icao_id = communication_partner_id;
				request->originator_id = communication_partner_id;
				((L2HeaderLinkEstablishmentRequest*) request->getHeaders().at(1))->icao_dest_id = own_id;
				request->dest_id = own_id;
				// Receive it on the BC.
				bc_manager->receiveFromLower(request);
				// Fetch the now-instantiated P2P manager.
				LinkManager* p2p_manager = mac->getLinkManager(communication_partner_id);
				// And increment time until it has sent the reply.
				CPPUNIT_ASSERT_EQUAL(size_t(0), phy_layer->outgoing_packets.size());
				while (!p2p_manager->control_messages.empty())
					mac->update(1);
				CPPUNIT_ASSERT_EQUAL(size_t(1), phy_layer->outgoing_packets.size());
				L2Packet* reply = phy_layer->outgoing_packets.at(0);
				CPPUNIT_ASSERT_EQUAL(own_id, reply->getOrigin());
				CPPUNIT_ASSERT_EQUAL(communication_partner_id, reply->getDestination());
//				coutd.setVerbose(false);
			}
		
		CPPUNIT_TEST_SUITE(LinkManagerTests);
			CPPUNIT_TEST(testTrafficEstimate);
			CPPUNIT_TEST(testNewLinkEstablishment);
			CPPUNIT_TEST(testComputeProposal);
			CPPUNIT_TEST(testTransmissionSlotOnUnestablishedLink);
			CPPUNIT_TEST(testNewLinkRequest);
			CPPUNIT_TEST(testOnTransmissionSlot);
			CPPUNIT_TEST(testTriggerNewLinkRequest);
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
		CPPUNIT_TEST_SUITE_END();
	};
}