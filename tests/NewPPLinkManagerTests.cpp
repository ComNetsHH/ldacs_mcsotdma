//
// Created by Sebastian Lindner on 12/21/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../NewPPLinkManager.hpp"
#include "../SHLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class NewPPLinkManagerTests : public CppUnit::TestFixture {
	private:
		TestEnvironment *env;
		uint32_t planning_horizon;
		NewPPLinkManager *pp;
		SHLinkManager *sh;
		MacId own_id, partner_id;		
		ReservationManager *reservation_manager;
		MCSOTDMA_Mac *mac;

	public:
		void setUp() override {
			own_id = MacId(42);
			partner_id = MacId(43);
			env = new TestEnvironment(own_id, partner_id, true);
			pp = (NewPPLinkManager*) env->mac_layer->getLinkManager(partner_id);
			sh = (SHLinkManager*) env->mac_layer->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
			mac = env->mac_layer;
			reservation_manager = env->mac_layer->getReservationManager();
		}

		void tearDown() override {
			delete env;
		}

		/** When new data is reported and the link is not established, establishment should be triggered. */
		void testStartLinkEstablishment() {
			// initially no link requests and no scheduled broadcast slot
			CPPUNIT_ASSERT_EQUAL(size_t(0), sh->link_requests.size());	
			CPPUNIT_ASSERT_EQUAL(false, sh->next_broadcast_scheduled);		
			// trigger link establishment
			mac->notifyOutgoing(100, partner_id);
			// now there should be a link request
			CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
			// and a scheduled broadcast slot
			CPPUNIT_ASSERT_EQUAL(true, sh->next_broadcast_scheduled);
		}

		/** When new data is reported and the link is *not unestablished*, establishment should *not* be triggered. */
		void testDontStartLinkEstablishmentIfNotUnestablished() {
			// initially no link requests and no scheduled broadcast slot
			CPPUNIT_ASSERT_EQUAL(size_t(0), sh->link_requests.size());	
			CPPUNIT_ASSERT_EQUAL(false, sh->next_broadcast_scheduled);		
			// trigger link establishment
			mac->notifyOutgoing(100, partner_id);
			// now there should be a link request
			CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
			// and a scheduled broadcast slot
			CPPUNIT_ASSERT_EQUAL(true, sh->next_broadcast_scheduled);
			unsigned int broadcast_slot = sh->next_broadcast_slot;
			CPPUNIT_ASSERT_GREATER(uint(0), broadcast_slot);
			
			// now, notify about even more data
			mac->notifyOutgoing(100, partner_id);
			// which shouldn't have changed anything
			CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
			CPPUNIT_ASSERT_EQUAL(true, sh->next_broadcast_scheduled);
			CPPUNIT_ASSERT_EQUAL(broadcast_slot, sh->next_broadcast_slot);
		}

		void testSlotSelection() {
			unsigned int burst_length = 2, burst_length_tx = 1;
			auto proposals = pp->slotSelection(pp->proposal_num_frequency_channels, pp->proposal_num_time_slots, burst_length, burst_length_tx);
			// proposals is a map <channel, <time slots>>
			// so there should be as many items as channels
			CPPUNIT_ASSERT_EQUAL(size_t(pp->proposal_num_frequency_channels + 1), proposals.size()); // +1 because of the reply slot on the SH 
			for (const auto pair : proposals) {
				const auto *channel = pair.first;				
				const auto &slots = pair.second;								
				if (channel->isPP()) {
					// per channel, as many slots as was the target					
					CPPUNIT_ASSERT_EQUAL(size_t(pp->proposal_num_time_slots), slots.size());
					// and these should be starting at the minimum offset
					// but then all the same for the different channels (don't have to be consecutive in time)
					for (size_t t = 0; t < pp->proposal_num_time_slots; t++)
						CPPUNIT_ASSERT_EQUAL((unsigned int) (2*pp->min_offset_to_allow_processing + t), slots.at(t)); // *2 because of link reply that in this case must be scheduled at the min_offset slot
				// where the SH is special, as only a single slot for the link reply should've been selected
				} else {
					CPPUNIT_ASSERT_EQUAL(size_t(1), slots.size());
					CPPUNIT_ASSERT_EQUAL((unsigned int) pp->min_offset_to_allow_processing, slots.at(0));
				}
			}
		}

		/** When the link request is being transmitted, this should trigger slot selection. 
		 * The number of proposed resources should match the settings, and these should all be idle. 
		 * Afterwards, they should be locked. */
		void testSlotSelectionThroughLinkRequestTransmission() {
			env->rlc_layer->should_there_be_more_broadcast_data = false;
			// trigger link establishment
			mac->notifyOutgoing(100, partner_id);
			unsigned int broadcast_slot = sh->next_broadcast_slot;
			CPPUNIT_ASSERT_GREATER(uint(0), broadcast_slot);
			// proceed until request is sent
			CPPUNIT_ASSERT_EQUAL(size_t(0), env->phy_layer->outgoing_packets.size());
			for (size_t t = 0; t < broadcast_slot; t++) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(false, sh->next_broadcast_scheduled);
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());
			// get proposed resources
			auto *link_request = env->phy_layer->outgoing_packets.at(0);
			int request_index = link_request->getRequestIndex();
			CPPUNIT_ASSERT(request_index != -1);			
			const auto *request_payload = (LinkManager::LinkRequestPayload*) link_request->getPayloads().at(request_index);
			const auto &proposed_resources = request_payload->proposed_resources;
			CPPUNIT_ASSERT_EQUAL(size_t(pp->proposal_num_frequency_channels + 1), proposed_resources.size()); // +1 because of the reply slot on the SH 
			// they should all be locked
			for (auto pair : proposed_resources) {
				const auto *frequency_channel = pair.first;
				if (frequency_channel->isSH())
					continue;
				const auto &time_slots = pair.second;
				CPPUNIT_ASSERT_EQUAL(size_t(pp->proposal_num_time_slots), time_slots.size());
				const ReservationTable *table = reservation_manager->getReservationTable(frequency_channel);
				for (size_t t : time_slots) 
					CPPUNIT_ASSERT_EQUAL(Reservation::LOCKED, table->getReservation(t).getAction());					
			}
		}

		/** When two links should be established, they should propose non-overlapping resources for each. */
		void testTwoSlotSelections() {
			env->rlc_layer->should_there_be_more_broadcast_data = false;
			((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setEnableBeacons(false);
			// trigger link establishments			
			mac->notifyOutgoing(100, partner_id);			
			mac->notifyOutgoing(100, MacId(partner_id.getId() + 1));			
			// proceed until both requests have been transmitted
			size_t num_slots = 0, max_slots = 100;			
			while (env->phy_layer->outgoing_packets.size() < 1 && num_slots++ < max_slots) {				
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();				
			}			
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());
			CPPUNIT_ASSERT_LESS(max_slots, num_slots);
			// proposals is a map <channel, <time slots>>
			auto *link_request = env->phy_layer->outgoing_packets.at(0);			
			int request_index_1 = -1, request_index_2 = -1;			
			for (size_t i = 0; i < link_request->getHeaders().size(); i++) {
				if (link_request->getHeaders().at(i)->frame_type == L2Header::FrameType::link_establishment_request) {
					if (request_index_1 == -1)
						request_index_1 = i;
					else 
						request_index_2 = i;
				}
			}			
			auto &proposed_resources_1 = ((LinkManager::LinkRequestPayload*) link_request->getPayloads().at(request_index_1))->proposed_resources;			 						
			auto &proposed_resources_2 = ((LinkManager::LinkRequestPayload*) link_request->getPayloads().at(request_index_2))->proposed_resources;
			// so there should be as many items as channels
			CPPUNIT_ASSERT_EQUAL(size_t(pp->proposal_num_frequency_channels + 1), proposed_resources_1.size()); // +1 because of the reply slot on the SH 
			CPPUNIT_ASSERT_EQUAL(size_t(pp->proposal_num_frequency_channels + 1), proposed_resources_2.size()); // +1 because of the reply slot on the SH 
			for (const auto pair : proposed_resources_1) {
				const auto *channel = pair.first;
				if (channel->isSH())
					continue;
				const auto &slots_1 = pair.second;
				const auto &slots_2 = proposed_resources_2[channel];
				// and per channel, as many slots as was the target
				CPPUNIT_ASSERT_EQUAL(size_t(pp->proposal_num_time_slots), slots_1.size());
				CPPUNIT_ASSERT_EQUAL(size_t(pp->proposal_num_time_slots), slots_2.size());
				// and for this channel, the time slots shouldn't overlap				
				for (size_t i = 0; i < slots_1.size(); i++) 
					for (size_t j = 0; j < slots_2.size(); j++) 
						CPPUNIT_ASSERT(slots_1.at(i) != slots_2.at(j));
			}
		}

		/** Calling notifyOutgoing should update the outgoing traffic estimate. */
		void testOutgoingTrafficEstimateEverySlot() {
			unsigned int num_bits = 512;
			size_t num_slots = pp->burst_offset * 10;
			for (size_t t = 0; t < num_slots; t++) {
				mac->update(1);
				pp->notifyOutgoing(num_bits);				
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(num_bits, (unsigned int) pp->outgoing_traffic_estimate.get());
		}

		/** Calling notifyOutgoing should update the outgoing traffic estimate. 
		 * If nothing is reported during one time slot, then a zero is put instead.
		 * */
		void testOutgoingTrafficEstimateEverySecondSlot() {
			unsigned int num_bits = 512;
			size_t num_slots = pp->burst_offset * 10;
			for (size_t t = 0; t < num_slots; t++) {
				mac->update(1);
				if (t % 2 == 0)
					pp->notifyOutgoing(num_bits);				
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(num_bits / 2, (unsigned int) pp->outgoing_traffic_estimate.get());
		}

		/** When in total, fewer resources than the burst offset are requested, then just those should be used. */
		void testTxRxSplitSmallerThanBurstOffset() {
			unsigned int tx_req = 5, rx_req = 5, burst_offset = 15;
			auto split = pp->getTxRxSplit(tx_req, rx_req, burst_offset);
			CPPUNIT_ASSERT_EQUAL(tx_req, split.first);
			CPPUNIT_ASSERT_EQUAL(rx_req, split.second);
		}

		/** When in total, as many resources as the burst offset are requested, then just those should be used. */
		void testTxRxSplitEqualToBurstOffset() {
			unsigned int tx_req = 5, rx_req = 5, burst_offset = 10;
			auto split = pp->getTxRxSplit(tx_req, rx_req, burst_offset);
			CPPUNIT_ASSERT_EQUAL(tx_req, split.first);
			CPPUNIT_ASSERT_EQUAL(rx_req, split.second);
		}

		/** When in total, more resources than the burst offset are requested, then a fair split should be used. */
		void testTxRxSplitMoreThanBurstOffset() {
			unsigned int tx_req = 5, rx_req = 5, burst_offset = 6;
			auto split = pp->getTxRxSplit(tx_req, rx_req, burst_offset);
			CPPUNIT_ASSERT_EQUAL(burst_offset/2, split.first);
			CPPUNIT_ASSERT_EQUAL(burst_offset/2, split.second);
		}

		/** When in total, more resources than the burst offset are requested, then a fair split should be used. */
		void testTxRxSplitMoreThanBurstOffsetOneSided() {
			unsigned int tx_req = 10, rx_req = 5, burst_offset = 6;
			auto split = pp->getTxRxSplit(tx_req, rx_req, burst_offset);
			CPPUNIT_ASSERT_EQUAL(uint(4), split.first);
			CPPUNIT_ASSERT_EQUAL(uint(2), split.second);
		}

		/** When in total, more resources than the burst offset are requested, then a fair split should be used. */
		void testTxRxSplitMoreThanBurstOffsetOtherSide() {
			unsigned int tx_req = 5, rx_req = 10, burst_offset = 6;
			auto split = pp->getTxRxSplit(tx_req, rx_req, burst_offset);
			CPPUNIT_ASSERT_EQUAL(uint(2), split.first);
			CPPUNIT_ASSERT_EQUAL(uint(4), split.second);
		}

		/** At transmission of the link request, the reply reception should be reserved on the SH and for the receiver. */
		void testReplySlotReserved() {
			env->rlc_layer->should_there_be_more_broadcast_data = false;
			// trigger link establishment
			mac->notifyOutgoing(100, partner_id);
			unsigned int broadcast_slot = sh->next_broadcast_slot;
			CPPUNIT_ASSERT_GREATER(uint(0), broadcast_slot);
			// proceed until request is sent
			CPPUNIT_ASSERT_EQUAL(size_t(0), env->phy_layer->outgoing_packets.size());
			for (size_t t = 0; t < broadcast_slot; t++) {
				mac->update(1);
				mac->execute();
				mac->onSlotEnd();
			}
			CPPUNIT_ASSERT_EQUAL(false, sh->next_broadcast_scheduled);
			CPPUNIT_ASSERT_EQUAL(size_t(1), env->phy_layer->outgoing_packets.size());
			// now the reply slot should be awaited on the SH
			auto *sh_table = reservation_manager->getBroadcastReservationTable();
			int reply_slot = 0;
			for (int t = 0; t < planning_horizon; t++) {
				if (sh_table->isUtilized(t)) {
					reply_slot = t;
					CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), sh_table->getReservation(t));
					break;
				}
			}
			CPPUNIT_ASSERT_GREATER(0, reply_slot);
			size_t num_rx_reservations = 0;
			for (auto *rx_table : reservation_manager->getRxTables()) {
				if (!rx_table->isIdle(reply_slot))
					num_rx_reservations++;
				for (int t = 0; t < planning_horizon; t++) {
					if (t == reply_slot)
						CPPUNIT_ASSERT_EQUAL(Reservation(partner_id, Reservation::RX), rx_table->getReservation(t));
					else
						CPPUNIT_ASSERT_EQUAL(true, rx_table->isIdle(t));
				}
			}
			CPPUNIT_ASSERT_EQUAL(size_t(1), num_rx_reservations);
		}

		/** When no reply has been received in the advertised slot, link establishment should be re-triggered. */
		void testReplySlotPassed() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When an expected reply has been received, the link status should reflect that. */
		void testReplyReceived() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When a link request is received, but the indicated reply slot is not suitable, this should trigger link establishment. */
		void testRequestReceivedButReplySlotUnsuitable() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When a link request is received, but none of the proposed resources are suitable, this should trigger link establishment. */
		void testRequestReceivedButProposedResourcesUnsuitable() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When a link request is received, the reply slot is suitable, a proposed resource is suitable, then this should be selected and the reply slot scheduled. */
		void testProcessRequestAndScheduleReply() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}	

		/** When a link reply is received, this should unschedule any own link replies currently scheduled. 
		 * The first reply is handled.
		 */
		void testUnscheduleOwnReplyUponReplyReception() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When a link reply is received, this should establish the link on the indicated resources. */
		void testEstablishLinkUponReplyReception() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}

		/** When the first burst has been handled, this should be reflected in both users' stati. */
		void testEstablishLinkUponFirstBurst() {
			bool is_implemented = false;
			CPPUNIT_ASSERT_EQUAL(true, is_implemented);
		}
 


	CPPUNIT_TEST_SUITE(NewPPLinkManagerTests);
		CPPUNIT_TEST(testStartLinkEstablishment);
		CPPUNIT_TEST(testDontStartLinkEstablishmentIfNotUnestablished);		
		CPPUNIT_TEST(testSlotSelection);
		CPPUNIT_TEST(testSlotSelectionThroughLinkRequestTransmission);
		CPPUNIT_TEST(testTwoSlotSelections);		
		CPPUNIT_TEST(testOutgoingTrafficEstimateEverySlot);		
		CPPUNIT_TEST(testOutgoingTrafficEstimateEverySecondSlot);				
		CPPUNIT_TEST(testTxRxSplitSmallerThanBurstOffset);
		CPPUNIT_TEST(testTxRxSplitEqualToBurstOffset);			
		CPPUNIT_TEST(testTxRxSplitMoreThanBurstOffset);
		CPPUNIT_TEST(testTxRxSplitMoreThanBurstOffsetOneSided);		
		CPPUNIT_TEST(testTxRxSplitMoreThanBurstOffsetOtherSide);		
		// CPPUNIT_TEST(testReplySlotPassed);
		// CPPUNIT_TEST(testReplyReceived);
		// CPPUNIT_TEST(testRequestReceivedButReplySlotUnsuitable);
		// CPPUNIT_TEST(testRequestReceivedButProposedResourcesUnsuitable);
		// CPPUNIT_TEST(testProcessRequestAndScheduleReply);
		// CPPUNIT_TEST(testUnscheduleOwnReplyUponReplyReception);
		// CPPUNIT_TEST(testEstablishLinkUponReplyReception);
		// CPPUNIT_TEST(testEstablishLinkUponFirstBurst);
	CPPUNIT_TEST_SUITE_END();
	};
}