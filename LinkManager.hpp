//
// Created by Sebastian Lindner on 10.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP

#include "MacId.hpp"
#include <L2Packet.hpp>
#include "ReservationManager.hpp"
#include "BeaconPayload.hpp"
#include "MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class MCSOTDMA_Mac;
    // Implemented in its own files for better readability and testing.
    class LinkManagementEntity;
	
	/**
	 * A LinkManager is responsible for a single communication link.
	 * It is notified by a QueueManager of new packets, and utilizes a ReservationManager to make slot reservations.
	 */
	class LinkManager : public L2PacketSentCallback {
			
		friend class LinkManagerTests;
		friend class BCLinkManagerTests;
		friend class MCSOTDMA_MacTests;
		friend class SystemTests;

		friend class LinkManagementEntity;
			
		public:
			enum Status {
				/** Everything is OK. */
				link_established,
				/** Link has not been established yet. */
				link_not_established,
				/** Link establishment request has been prepared and we're waiting for the reply. */
				awaiting_reply,
				/** Link establishment reply has been sent and we're waiting for the first message. */
				reply_sent,
				/** Link renewal has been completed. After expiry, the new reservations take action. */
				link_renewal_complete
			};
			
			LinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac);
			virtual ~LinkManager();
			
			/**
			 * @return The link ID that is managed.
			 */
			const MacId& getLinkId() const;
			
			/**
			 * When a new packet for this link comes in from the upper layers, this notifies the LinkManager.
			 * Applies P2P slot selection.
			 */
			virtual void notifyOutgoing(unsigned long num_bits);
			
			/**
			 * @param num_slots Number of consecutive slots that may be used for this transmission.
			 * @return A data packet that should now be sent.
			 */
			virtual L2Packet* onTransmissionBurst(unsigned int num_slots);
			
			/**
			 * When a packet on this link comes in from the PHY, this notifies the LinkManager.
			 */
			void receiveFromLower(L2Packet*& packet);
			
			/**
			 * @return The current, computed traffic estimate from a moving average over some window of past values.
			 */
			double getCurrentTrafficEstimate() const;
			
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
			void packetBeingSentCallback(TUHH_INTAIRNET_MCSOTDMA::L2Packet* packet) override;
			
			/**
			 * Assign both FrequencyChannel and corresponding ReservationTable.
			 * @param channel
			 */
			void assign(const FrequencyChannel* channel);
			
			/**
			 * Schedules a link reply.
			 * @param reply
			 * @param slot_offset
			 */
			void scheduleLinkReply(L2Packet* reply, int32_t slot_offset, unsigned int timeout, unsigned int offset, unsigned int length);

            virtual void update(uint64_t num_slots);
		
		protected:
			/**
			 * Makes reservations.
			 * @param timeout Number of repetitions.
			 * @param init_offset First offset to start with.
			 * @param offset Increment offset each repetition.
			 * @param length Number of slots.
			 * @param action
			 */
			void markReservations(unsigned int timeout, unsigned int init_offset, unsigned int offset, unsigned int length, const MacId& target_id, Reservation::Action action);
			
			/**
			 * When a beacon packet comes in from the PHY, this processes it.
			 * @oaram origin_id
			 * @param header
			 * @param payload
			 */
			virtual void processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload);
			
			/**
			 * When a broadcast packet comes in from the PHY, this processes it.
			 * @param origin
			 * @param header
			 */
			virtual void processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header);
			
			/**
			 * When a unicast packet comes in from the PHY, this processes it.
			 * @param header
			 * @param payload
			 */
			void processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload);
			
			/**
			 * Processes the base header of each incoming packet.
			 * @param header
			 */
			void processIncomingBase(L2HeaderBase*& header);
			
			/**
			 * Encodes this user's reserved transmission slots.
			 * @param max_bits Maximum number of bits this payload should encompass.
			 * @return
			 */
			BeaconPayload* computeBeaconPayload(unsigned long max_bits) const;
			
			/**
			 * Checks validity and delegates to set{Base,Beacon,Broadcast,Unicast,Request}HeaderFields.
			 * @param header The header whose fields shall be set.
			 */
			void setHeaderFields(L2Header* header);
			void setBaseHeaderFields(L2HeaderBase*& header);
			virtual void setBeaconHeaderFields(L2HeaderBeacon*& header) const;
			virtual void setBroadcastHeaderFields(L2HeaderBroadcast*& header) const;
			void setUnicastHeaderFields(L2HeaderUnicast*& header) const;
			
			/**
			 * @return Based on the current traffic estimate and the current data rate, calculate the number of slots that should be reserved for this link.
			 */
			unsigned int estimateCurrentNumSlots() const;
			
			void updateTrafficEstimate(unsigned long num_bits);
			
			/**
			 * @param start
			 * @param end
			 * @return Uniformly drawn random integer from [start, end] (exclusive).
			 */
			static size_t getRandomInt(size_t start, size_t end) ;
			
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
			const FrequencyChannel* current_channel = nullptr;
			/** A link is assigned on one particular frequency channel's reservation table. It may be nullptr unless the link_establishment status is `link_established`. */
			ReservationTable* current_reservation_table = nullptr;
			/** Current traffic estimate of this link. */
            MovingAverage traffic_estimate;
            /** Takes care of link management. It resides in its own class to modularize the code. */
			LinkManagementEntity* lme = nullptr;

	};

    inline std::ostream& operator<<(std::ostream& stream, const LinkManager& lm) {
        return stream << "LinkManager(" << lm.getLinkId() << ")";
    }
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
