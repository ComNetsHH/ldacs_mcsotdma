//
// Created by Sebastian Lindner on 10.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP

#include "MacId.hpp"
#include <L2Packet.hpp>
#include <cmath>
#include "ReservationManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class MCSOTDMA_Mac;
	
	/**
	 * A LinkManager is responsible for a single communication link.
	 * It is notified by a QueueManager of new packets, and utilizes a ReservationManager to make slot reservations.
	 */
	class LinkManager : public L2PacketSentCallback {
			
		friend class LinkManagerTests;
		friend class MCSOTDMA_MacTests;
			
		public:
			
			/**
			 * Implements a link establishment request payload that encodes proposed frequency channels and slots.
			 */
			class ProposalPayload : public L2Packet::Payload {
				public:
					ProposalPayload(unsigned int num_freq_channels, unsigned int num_slots) : target_num_channels(num_freq_channels), target_num_slots(num_slots), num_slots_per_candidate(1) {
						if (target_num_slots > pow(2, 4))
							throw std::runtime_error("Cannot encode more than 16 candidate slots.");
					}
					
					/** Copy constructor. */
					ProposalPayload(const ProposalPayload& other)
						: proposed_channels(other.proposed_channels), proposed_slots(other.proposed_slots), num_candidates(other.num_candidates),
						target_num_channels(other.target_num_channels), target_num_slots(other.target_num_slots), num_slots_per_candidate(other.num_slots_per_candidate) {}
					
					unsigned int getBits() const override {
						return 8 * target_num_channels // 1B per frequency channel
						+ 8*target_num_slots // 1B per candidate
						+ 4*target_num_slots // number of actual candidates per channel
						+ 8; // 1B to denote candidate slot length
					}
					
					std::vector<const FrequencyChannel*> proposed_channels;
					/** Starting slots. */
					std::vector<unsigned int> proposed_slots;
					/** Actual number of candidates per frequency channel. */
					std::vector<unsigned int> num_candidates;
					/** Target number of frequency channels to propose. */
					unsigned int target_num_channels;
					/** Target number of slots to propose. */
					unsigned int target_num_slots;
					/** Number of slots to reserve. */
					unsigned int num_slots_per_candidate;
			};
			
			/**
			 * Implements a beacon payload that encodes a user's reservations.
			 */
			class BeaconPayload : public L2Packet::Payload {
				public:
					static constexpr unsigned int BITS_PER_SLOT = 8, BITS_PER_CHANNEL = 8;
					
					explicit BeaconPayload(const MacId& beacon_owner_id) : beacon_owner_id(beacon_owner_id) {}
					~BeaconPayload() override {
						for (const auto& pair : local_reservations)
							delete pair.second;
					}
					
					unsigned int getBits() const override {
						unsigned int bits = 0;
						for (auto pair : local_reservations) {
							bits += pair.second->countReservedTxSlots(beacon_owner_id) * BITS_PER_SLOT;
							bits += BITS_PER_CHANNEL;
						}
						return bits;
					}
					
					std::vector<std::pair<FrequencyChannel, ReservationTable*>> local_reservations;
					const MacId beacon_owner_id;
			};
			
			enum Status {
				/** Everything is OK. */
				link_established,
				/** Link has not been established yet. */
				link_not_established,
				/** Link establishment request has been prepared and we're waiting for the reply. */
				awaiting_reply
			};
			
			LinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac);
			
			/**
			 * @return The link ID that is managed.
			 */
			const MacId& getLinkId() const;
			
			/**
			 * When a new packet for this link comes in from the upper layers, this notifies the LinkManager.
			 */
			void notifyOutgoing(unsigned long num_bits);
			
			/**
			 * @param num_slots Number of consecutive slots that may be used for this transmission.
			 * @return A data packet that should now be sent.
			 */
			L2Packet* onTransmissionSlot(unsigned int num_slots);
			
			/**
			 * When a packet on this link comes in from the PHY, this notifies the LinkManager.
			 */
			void receiveFromLower(L2Packet* packet);
			
			/**
			 * @param num_candidate_channels Number of distinct frequency channels that should be proposed.
			 * @param num_candidate_slots Number of distinct time slots per frequency channel that should be proposed.
			 */
			void setProposalDimension(unsigned int num_candidate_channels, unsigned int num_candidate_slots);
			
			/**
			 * @return The current, computed traffic estimate from a moving average over some window of past values.
			 */
			double getCurrentTrafficEstimate() const;
			
			const unsigned int& getTrafficEstimateWindowSize() const;
			
			/**
			 * @return The number of slot reservations that have been made but are yet to arrive.
			 */
			unsigned int getNumPendingReservations() const;
			
			/**
			 * @param start_slot The minimum slot offset to start the search.
			 * @param reservation
			 * @return The slot offset until the earliest reservation that corresponds to the one provided.
			 * @throws std::runtime_error If no reservation of this kind is found.
			 */
			int32_t getEarliestReservationSlotOffset(int32_t start_slot, const Reservation& reservation) const;
			
			/**
			 * From L2PacketSentCallback interface: when a packet leaves the layer, the LinkManager may be notified.
			 * This is used to set header fields, and to compute link request proposals.
			 * @param packet
			 */
			void notifyPacketBeingSent(TUHH_INTAIRNET_MCSOTDMA::L2Packet* packet) override;
			
			/**
			 * @param reservation_timeout Number of repetitions a reservation remains valid for.
			 */
			void setReservationTimeout(unsigned int reservation_timeout);
			
			/**
			 * @param reservation_offset Number of slots until the next transmission. Should be set to the P2P frame length, or dynamically for broadcast-type transmissions.
			 */
			void setReservationOffset(unsigned int reservation_offset);
		
		protected:
			
			/**
			 * Prepares a link request and injects it into the upper layers.
			 */
			void requestNewLink();
			
			/**
			 * Upon a transmission slot, the link establishment request payload must be computed.
			 * @return The modified data packet, now containing the request payload.
			 */
			L2Packet* prepareLinkEstablishmentRequest();
			
			/**
			 * @return A new beacon.
			 */
			L2Packet* prepareBeacon();
			
			/**
			 * When a link estabishment request comes in from the PHY, this processes it.
			 * @param header
			 * @param payload
			 * @return Out of the proposed (freq. channel, slot offset) candidates, those candidates are returned that are idle for us.
			 */
			std::vector<std::pair<const FrequencyChannel*, unsigned int>> processIncomingLinkEstablishmentRequest(L2HeaderLinkEstablishmentRequest*& header, ProposalPayload*& payload);
			
			/**
			 * When a link establishment reply comes in from the PHY, this processes it.
			 * @param header
			 */
			void processIncomingLinkEstablishmentReply(L2HeaderLinkEstablishmentReply*& header);
			
			/**
			 * When a beacon packet comes in from the PHY, this processes it.
			 * @oaram origin_id
			 * @param header
			 * @param payload
			 */
			void processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload);
			
			/**
			 * When a broadcast packet comes in from the PHY, this processes it.
			 * @param header
			 */
			void processIncomingBroadcast(L2HeaderBroadcast*& header);
			
			/**
			 * When a unicast packet comes in from the PHY, this processes it.
			 * @param header
			 * @param payload
			 */
			void processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload);
			
			/**
			 * Processes the base header of each incoming packet.
			 * @param header
			 * @return The originator's ID.
			 */
			MacId processIncomingBase(L2HeaderBase*& header);
			
			/**
			 * @return A payload that should accompany a link request.
			 */
			LinkManager::ProposalPayload* computeRequestProposal() const;
			
			/**
			 * Encodes this user's reserved transmission slots.
			 * @param max_bits Maximum number of bits this payload should encompass.
			 * @return
			 */
			LinkManager::BeaconPayload* computeBeaconPayload(unsigned long max_bits) const;
			
			/**
			 * Checks validity and delegates to set{Base,Beacon,Broadcast,Unicast,Request}HeaderFields.
			 * @param header The header whose fields shall be set.
			 */
			void setHeaderFields(L2Header* header);
			void setBaseHeaderFields(L2HeaderBase* header);
			void setBeaconHeaderFields(L2HeaderBeacon* header) const;
			void setBroadcastHeaderFields(L2HeaderBroadcast* header) const;
			void setUnicastHeaderFields(L2HeaderUnicast* header) const;
			void setRequestHeaderFields(L2HeaderLinkEstablishmentRequest* header) const;
			
			/**
			 * @return Based on the current traffic estimate and the current data rate, calculate the number of slots that should be reserved for this link.
			 */
			unsigned long estimateCurrentNumSlots() const;
			
			void updateTrafficEstimate(unsigned long num_bits);
			
		protected:
			/** The communication partner's ID, whose link is managed. */
			const MacId link_id;
			/** Points to the reservation manager. */
			ReservationManager* reservation_manager;
			/** Points to the MAC sublayer. */
			MCSOTDMA_Mac* mac;
			/** Link establishment status. */
			Status link_establishment_status;
			/** A link is assigned on one particular frequency channel. It may be nullptr unless the link_establishment status is `link_established`. */
			FrequencyChannel* current_channel = nullptr;
			/** A link is assigned on one particular frequency channel's reservation table. It may be nullptr unless the link_establishment status is `link_established`. */
			ReservationTable* current_reservation_table = nullptr;
			/** The minimum number of slots a proposed slot should be in the future. */
			const int32_t minimum_slot_offset_for_new_slot_reservations = 1;
			/** The number of frequency channels that should be proposed when a new link request is prepared. */
			unsigned int num_proposed_channels = 2;
			/** The number of time slots that should be proposed when a new link request is prepared. */
			unsigned int num_proposed_slots = 3;
			/** The number of past values to consider for the traffic estimate. */
			const unsigned int traffic_estimate_num_values = 20;
			/** Whenever a LinkManager is notified of new data for its link, a number of values are saved so that a traffic estimate can be calculated. */
			std::vector<unsigned long long> traffic_estimate_queue_lengths;
			/** Keeps track of the index that should be updated next in the `traffic_estimate_queue_lengths`. */
			size_t traffic_estimate_index = 0;
			/** Keeps track of the number of slot reservations that have been made but are yet to arrive. */
			unsigned int num_pending_reservations = 0;
			/** Keeps a copy of the last proposal, so that reservations can be made when the proposal is accepted. */
			LinkManager::ProposalPayload* last_proposal = nullptr;
			/** Number of repetitions a reservation remains valid for. */
			unsigned int current_reservation_timeout = 1, reservation_timeout = 1;
			/** When a reservation timeout reaches this threshold, a new link request is prepared. */
			int TIMEOUT_THRESHOLD_TRIGGER = -1;
			/** Number of slots occupied per transmission burst. */
			unsigned short current_reservation_slot_length = 1;
			/** Number of slots until the next transmission. Should be set to the P2P frame length, or dynamically for broadcast-type transmissions. */
			unsigned int current_reservation_offset = 10;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
