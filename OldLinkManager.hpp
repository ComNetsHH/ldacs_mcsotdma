//
// Created by Sebastian Lindner on 10.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_OLDLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_OLDLINKMANAGER_HPP

#include "MacId.hpp"
#include <L2Packet.hpp>
#include "ReservationManager.hpp"
#include "BeaconPayload.hpp"
#include "MovingAverage.hpp"
#include "LinkManager.hpp"
#include <random>
#include <stdint-gcc.h>

namespace TUHH_INTAIRNET_MCSOTDMA {

	class MCSOTDMA_Mac;

	// Implemented in its own files for better readability and testing.
	class LinkManagementEntity;

	/**
	 * A OldLinkManager is responsible for a single communication link.
	 * It is notified by a QueueManager of new packets, and utilizes a ReservationManager to make slot reservations.
	 */
	class OldLinkManager : public L2PacketSentCallback, public LinkManager {

		friend class LinkManagementEntity;

		friend class BCLinkManagementEntity;

		friend class LinkManagerTests;

		friend class BCLinkManagerTests;

		friend class MCSOTDMA_MacTests;

		friend class SystemTests;

		friend class LinkManagementEntityTests;

	public:
		OldLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac);

		virtual ~OldLinkManager();

		/**
		 * When a new packet for this link comes in from the upper layers, this notifies the OldLinkManager.
		 * Applies P2P slot selection.
		 */
		void notifyOutgoing(unsigned long num_bits) override;

		L2Packet* onTransmissionBurstStart(unsigned int burst_length) override;

		void onReceptionBurstStart(unsigned int burst_length) override;

		void onReceptionBurst(unsigned int remaining_burst_length) override;

		void onTransmissionBurst(unsigned int remaining_burst_length) override;

		/**
		 * When a packet on this link comes in from the PHY, this notifies the OldLinkManager.
		 */
		void onPacketReception(L2Packet*& packet) override;

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
		 * From L2PacketSentCallback interface: when a packet leaves the layer, the OldLinkManager may be notified.
		 * This is used to set header fields, and to compute link request proposals.
		 * @param packet
		 */
		void packetBeingSentCallback(TUHH_INTAIRNET_MCSOTDMA::L2Packet* packet) override;

		void onSlotEnd() override;

		void onSlotStart(uint64_t num_slots) override;

	protected:
		/**
		 * Makes reservations on the current reservation table.
		 * @param timeout Number of repetitions.
		 * @param init_offset Excluding initial offset: first slot used will be init_offset+offset.
		 * @param offset Increment offset each repetition.
		 * @param length Number of slots.
		 * @param action
		 */
		void markReservations(unsigned int timeout, unsigned int init_offset, unsigned int offset, unsigned int length, const MacId& target_id, Reservation::Action action);

		/**
		 * Makes reservations on the given reservation table.
		 * @param table
		 * @param timeout Number of repetitions.
		 * @param init_offset Excluding initial offset: first slot used will be init_offset+offset.
		 * @param offset Increment offset each repetition.
		 * @param reservation
		 * @return The offsets where reservations were made.
		 */
		std::vector<unsigned int> markReservations(ReservationTable* table, unsigned int timeout, unsigned int init_offset, unsigned int offset, const Reservation& reservation);

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
		virtual void processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload);

		/**
		 * Processes the base header of each incoming packet.
		 * @param header
		 */
		virtual void processIncomingBase(L2HeaderBase*& header);

		void processIncomingLinkRequest(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) override;

		virtual void processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload);

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
		size_t getRandomInt(size_t start, size_t end);

		/**
		 * Reassign both FrequencyChannel and corresponding ReservationTable.
		 * @param channel
		 */
		void reassign(const FrequencyChannel* channel);

	protected:
		/** Current traffic estimate of this link. */
		MovingAverage traffic_estimate;
		/** Whether this instance is the initiator of a link, i.e. sends the requests. */
		bool is_link_initiator = false;
		/** Takes care of link management. It resides in its own class to modularize the code. */
		LinkManagementEntity* lme = nullptr;
		size_t statistic_num_received_packets = 0,
			statistic_num_received_data_packets = 0,
			statistic_num_received_requests = 0,
			statistic_num_received_replies = 0,
			statistic_num_received_beacons = 0,
			statistic_num_received_broadcasts = 0,
			statistic_num_received_unicasts = 0,
			statistic_num_sent_packets = 0,
			statistic_num_sent_data_packets = 0,
			statistic_num_sent_requests = 0,
			statistic_num_sent_replies = 0,
			statistic_num_sent_beacons = 0,
			statistic_num_sent_broadcasts = 0,
			statistic_num_sent_unicasts = 0;

		std::random_device* random_device;
		std::mt19937 generator;

	};

	inline std::ostream& operator<<(std::ostream& stream, const OldLinkManager& lm) {
		return stream << "OldLinkManager(" << lm.getLinkId() << ")";
	}
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_OLDLINKMANAGER_HPP
