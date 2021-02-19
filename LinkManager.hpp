//
// Created by seba on 2/18/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP

#include <random>
#include <MacId.hpp>
#include <L2Packet.hpp>
#include "coutdebug.hpp"
#include "FrequencyChannel.hpp"
#include "ReservationTable.hpp"
#include "ReservationManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class MCSOTDMA_Mac;
	class BeaconPayload;

	/** LinkManager interface. */
	class LinkManager {

		friend class MCSOTDMA_MacTests;
		friend class SystemTests;

	public:
		enum Status {
			/** Everything is OK. */
			link_established,
			/** Link has not been established yet. */
			link_not_established,
			/** Link establishment request has been prepared and we're waiting for the reply. */
			awaiting_reply,
			/** Link establishment reply has been prepared and we're waiting for the first message. */
			awaiting_data_tx,
			/** Link renewal has been completed. After expiry, the new reservations take action. */
			link_renewal_complete
		};

		class LinkRequestPayload : public L2Packet::Payload {
		public:
			class Callback {
			public:
				virtual void linkRequestAboutToBeSent(LinkRequestPayload* payload) = 0;
			};

			LinkRequestPayload() = default;

			/** Copy constructor. */
			LinkRequestPayload(const LinkRequestPayload& other) : proposed_resources(other.proposed_resources) {}
			Payload* copy() const override {
				return new LinkRequestPayload(*this);
			}

			unsigned int getBits() const override {
				unsigned int num_bits = 0;
				for (const auto& item : proposed_resources) {
					num_bits += 8; // +1B per frequency channel
					num_bits += 8 * item.second.size(); // +1B per slot
				}
				return num_bits;
			}

			/** <channel, <start slots>>-map of proposed resources. */
			std::map<const FrequencyChannel*, std::vector<unsigned int>> proposed_resources;
			Callback *callback = nullptr;
			bool initial_request = false;
		};

		LinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : link_id(link_id), reservation_manager(reservation_manager), mac(mac),
		                                                                                                link_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */,
		                                                                                                random_device(new std::random_device), generator((*random_device)()){}

	    virtual ~LinkManager() {
			delete random_device;
		}

		/**
		 * When a packet has been received, this lets the LinkManager process it.
		 * @param packet The received packet.
		 */
		virtual void onPacketReception(L2Packet *&packet);

		/**
		 * Called when a reception burst starts.
		 * @param burst_length Number of slots that'll be received for.
		 */
		virtual void onReceptionBurstStart(unsigned int burst_length) = 0;

		/**
		 * Called each slot during a reception burst, *except* for the first slot, where onReceptionBurstStart() is called instead.
		 * @param remaining_burst_length Remaining number of slots that'll be received for.
		 */
		virtual void onReceptionBurst(unsigned int remaining_burst_length) = 0;

		/**
		 * Called when a transmission burst starts.
		 * @param burst_length Number of slots that'll be transmitted for.
		 * @return A packet that should be transmitted during this burst.
		 */
		virtual L2Packet* onTransmissionBurstStart(unsigned int burst_length) = 0;

		/**
		 * Called each slot during a transmission burst, *except* for the first slot, where onTransmissionBurstStart() is called instead.
		 * @param remaining_burst_length Remaining number of slots that'll be transmitted for.
		 */
		virtual void onTransmissionBurst(unsigned int remaining_burst_length) = 0;

		/**
		 * Called when upper layers notify the MAC of outgoing data for this link.
		 * @param num_bits
		 */
		virtual void notifyOutgoing(unsigned long num_bits) = 0;

		/**
		 * Called on slot start.
		 * @param num_slots Number of slots that have passed.
		 */
		virtual void onSlotStart(uint64_t num_slots) = 0;

		/**
		 * Called on slot end.
		 */
		virtual void onSlotEnd() = 0;

		void assign(const FrequencyChannel* channel);

		MacId getLinkId() const {
			return link_id;
		}

		void linkTxTable(ReservationTable *tx_table) {
			tx_tables.push_back(tx_table);
		}

		void linkRxTable(ReservationTable *rx_table) {
			rx_tables.push_back(rx_table);
		}

	protected:
		/**
		 * Locks given ReservationTable, as well as transmitter and receiver resources for the given candidate slots.
		 * @param start_slots Starting slot offsets.
		 * @param burst_length Number of first slots to lock the transmitter for.
		 * @param burst_length_tx Number of trailing slots to lock the receiver for
		 * @param table ReservationTable in which slots should be locked.
		 */
		void lock(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, ReservationTable* table);

		unsigned long getRandomInt(size_t start, size_t end);

		virtual void processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload);
		virtual void processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header);
		virtual void processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload);
		virtual void processIncomingBase(L2HeaderBase*& header);
		virtual void processIncomingLinkRequest(const L2HeaderLinkEstablishmentRequest*& header, const L2Packet::Payload*& payload, const MacId& origin);
		virtual void processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload);

	protected:
		MacId link_id;
		MCSOTDMA_Mac *mac;
		ReservationManager *reservation_manager = nullptr;
		const FrequencyChannel *current_channel = nullptr;
		ReservationTable *current_reservation_table = nullptr;
		std::vector<ReservationTable*> tx_tables;
		std::vector<ReservationTable*> rx_tables;
		/** Link establishment status. */
		Status link_status;
		std::random_device* random_device;
		std::mt19937 generator;

		unsigned long long statistic_num_received_packets, statistic_num_received_beacons, statistic_num_received_broadcasts, statistic_num_received_unicasts, statistic_num_received_requests, statistic_num_received_replies;
	};

	inline std::ostream& operator<<(std::ostream& stream, const LinkManager& lm) {
		return stream << "LinkManager(" << lm.getLinkId() << ")";
	}

	inline std::ostream& operator<<(std::ostream& stream, const LinkManager::Status& status) {
		std::string str;
		switch (status) {
			case LinkManager::link_renewal_complete: {
				str = "link_renewal_complete";
				break;
			}
			case LinkManager::link_not_established: {
				str = "link_not_established";
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
