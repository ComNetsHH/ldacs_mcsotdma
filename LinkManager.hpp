//
// Created by seba on 2/18/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP

#include <random>
#include <MacId.hpp>
#include <L2Packet.hpp>
#include <RngProvider.hpp>
#include "coutdebug.hpp"
#include "FrequencyChannel.hpp"
#include "ReservationTable.hpp"
#include "ReservationManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class MCSOTDMA_Mac;
	class BeaconPayload;

	/** LinkManager interface. */
	class LinkManager : public IRng {

		friend class MCSOTDMA_MacTests;
		friend class SystemTests;
		friend class ManyUsersTests;
		friend class ThirdPartyLinkTests;		
		friend class PPLinkManagerTests;

	public:
		enum Status {
			/** Everything is OK. */
			link_established,
			/** Link has not been established. */
			link_not_established,
			/** Awaiting time slot where the request is generated and transmitted. */
			awaiting_request_generation,
			/** We're waiting for a link reply. */
			awaiting_reply,
			/** We're waiting for the first transmission burst. */
			awaiting_data_tx
		};

		LinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : link_id(link_id), reservation_manager(reservation_manager), mac(mac),
		                                                                                                link_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */ {
		                                                                                                }

	    virtual ~LinkManager() = default;

		/**
		 * When a packet has been received, this lets the LinkManager process it.
		 * @param packet The received packet.
		 */
		virtual void onPacketReception(L2Packet *&packet);

		/**
		 * Called when a reception burst starts.		 
		 */
		virtual void onReceptionReservation();

		/**
		 * Called when a transmission burst starts.		 
		 * @return A packet that should be transmitted during this burst.
		 */
		virtual L2Packet* onTransmissionReservation() = 0;		

		/**
		 * Called when upper layers notify the MAC of outgoing data for this link.
		 * @param num_bits
		 */
		virtual void notifyOutgoing(unsigned long num_bits) = 0;

		/**
		 * Called on slot start.
		 * @param num_slots Number of slots that have passed.
		 */
		virtual void onSlotStart(uint64_t num_slots);

		/**
		 * Called on slot end.
		 */
		virtual void onSlotEnd();

		virtual void assign(const FrequencyChannel* channel);

		MacId getLinkId() const {
			return link_id;
		}
		
		// virtual void processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id);
		// virtual void processLinkReplyMessage(const L2HeaderLinkReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id);		

		virtual double getNumTxPerTimeSlot() const = 0;

		virtual bool isActive() const = 0;

		LinkManager::Status getLinkStatus() const;

	protected:		
		virtual void processBroadcastMessage(const MacId& origin, L2HeaderSH*& header);
		virtual void processUnicastMessage(L2HeaderPP*& header, L2Packet::Payload*& payload);
		
		/** 
		 * Called whenever a channel access is performed. Measures the number of slots since the last channel access and reports it to the MAC.
		 * @return Number of slots since the last channel access, i.e. the current MAC delay.
		 */
		unsigned int measureMacDelay();

		/** Called when a packet is successfully received. */
		virtual void receivedPacketThisSlot();

	protected:
		MacId link_id;
		MCSOTDMA_Mac *mac;
		ReservationManager *reservation_manager = nullptr;
		const FrequencyChannel *current_channel = nullptr;
		ReservationTable *current_reservation_table = nullptr;		
		/** Link establishment status. */
		Status link_status;
		/** To measure the MAC delay, keep track of the number of slots in-between channel accesses. */
		unsigned int time_slot_of_last_channel_access = 0;
		/** Flag to indicate that the current slot has been reserved for a packet reception. */
		bool expected_reception_this_slot = false;
		/** Flag to indicate that a packet has been received during the current slot. */
		bool received_packet_this_slot = false;
		/** Flag to indicate that an expected packet was not received and that this was reported to ARQ. */
		bool reported_missing_packet_to_arq = false;
	};

	inline std::ostream& operator<<(std::ostream& stream, const LinkManager& lm) {
		return stream << "LinkManager(" << lm.getLinkId() << ")";
	}

	inline std::ostream& operator<<(std::ostream& stream, const LinkManager::Status& status) {
		std::string str;
		switch (status) {
			case LinkManager::link_not_established: {
				str = "link_not_established";
				break;
			}
			case LinkManager::awaiting_request_generation: {
				str = "awaiting_request_generation";
				break;
			}
			case LinkManager::awaiting_reply: {
				str = "awaiting_reply";
				break;
			}
			case LinkManager::link_established: {
				str = "link_established";
				break;
			}
			case LinkManager::awaiting_data_tx: {
				str = "awaiting_data_tx";
				break;
			}
		}
		return stream << str;
	}
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
