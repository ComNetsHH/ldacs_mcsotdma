//
// Created by Sebastian Lindner on 16.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP

#include <IMac.hpp>
#include <L2Packet.hpp>
#include <IArq.hpp>
#include <IOmnetPluggable.hpp>
#include <Statistic.hpp>
#include "ReservationManager.hpp"
#include "LinkManager.hpp"
#include "MCSOTDMA_Phy.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * Implements the MAC interface.
	 */
	class MCSOTDMA_Mac : public IMac, public IOmnetPluggable {
	public:
		friend class MCSOTDMA_MacTests;
		friend class MCSOTDMA_PhyTests;
		friend class ThreeUsersTests;

		MCSOTDMA_Mac(const MacId& id, uint32_t planning_horizon);

		~MCSOTDMA_Mac() override;

		void notifyOutgoing(unsigned long num_bits, const MacId& mac_id) override;

		void passToLower(L2Packet* packet, unsigned int center_frequency) override;

		void receiveFromLower(L2Packet* packet, uint64_t center_frequency) override;

		/**
		 * @param id
		 * @return The LinkManager that manages the given 'id'.
		 */
		LinkManager* getLinkManager(const MacId& id);

		void passToUpper(L2Packet* packet) override;

		/** Notify this MAC that time has passed. */
		void update(uint64_t num_slots) override;

		/**
		 * Execute reservations valid in the current time slot.
		 * All users should have been updated before calling their executes s.t. time is synchronized.
		 * @return A pair of (num_transmissions, num_receptions) that were executed.
		 * */
		std::pair<size_t, size_t> execute();

        /**
         * Retrieve the reservation manager so that it can be configured.
         * @return The ReservationManager.
         */
        ReservationManager* getReservationManager();

        void onSlotEnd();

        const MCSOTDMA_Phy* getPhy() const;

		std::vector<std::pair<Reservation, const FrequencyChannel*>> getReservations(unsigned int t) const;

		void setBroadcastTargetCollisionProb(double value) override;

		void statisticReportBroadcastMessageDecoded() {
			stat_num_broadcast_msgs_decoded.increment();
		}
		void statisticReportBroadcastReceived() {
			stat_num_broadcasts_rcvd.increment();
		}
		void statisticReportUnicastMessageDecoded() {
			stat_num_unicast_msgs_decoded.increment();
		}
		void statisticReportUnicastReceived() {
			stat_num_unicasts_rcvd.increment();
		}
		void statisticReportLinkRequestReceived() {
			stat_num_requests_rcvd.increment();
		}
		void statisticReportLinkReplyReceived() {
			stat_num_replies_rcvd.increment();
		}
		void statisticReportBeaconReceived() {
			stat_num_beacons_rcvd.increment();
		}
		void statisticReportLinkInfoReceived() {
			stat_num_link_infos_rcvd.increment();
		}
		void statisticReportPacketSent() {
			stat_num_packets_sent.increment();
		}
		void statisticReportBroadcastSent() {
			stat_num_broadcasts_sent.increment();
		}
		void statisticReportUnicastSent() {
			stat_num_unicasts_sent.increment();
		}
		void statisticReportLinkRequestSent() {
			stat_num_requests_sent.increment();
		}
		void statisticReportLinkReplySent() {
			stat_num_replies_sent.increment();
		}
		void statisticReportBeaconSent() {
			stat_num_beacons_sent.increment();
		}
		void statisticReportLinkInfoSent() {
			stat_num_link_infos_sent.increment();
		}
		void statisticReportCancelledLinkRequest(size_t num_cancelled_requests) {
			stat_num_requests_cancelled.incrementBy(num_cancelled_requests);
		}
		void statisticReportNumActiveNeighbors(size_t val) {
			stat_num_active_neighbors.capture(val);
		}
		void statisticReportMinBeaconOffset(size_t val) {
			stat_min_beacon_offset.capture(val);
		}
		void statisticReportContention(double val) {
			stat_contention.capture(val);
		}
		void statisticReportCongestion(double val) {
			stat_congestion.capture(val);
		}
		void statisticReportBroadcastCandidateSlots(size_t val) {
			stat_broadcast_candidate_slots.capture(val);
		}

	protected:
		/**
		 * Define what happens when a particular FrequencyChannel should be listened on during this time slot.
		 * @param channel
		 */
		void onReceptionSlot(const FrequencyChannel* channel);

		/** Keeps track of transmission resource reservations. */
		ReservationManager* reservation_manager;
		/** Maps links to their link managers. */
		std::map<MacId, LinkManager*> link_managers;
		const size_t num_transmitters = 1, num_receivers = 2;
		/** Holds the current belief of neighbor positions. */
		std::map<MacId, CPRPosition> position_map;
		std::map<uint64_t, std::vector<L2Packet*>> received_packets;

		// Statistics
		Statistic stat_num_packets_rcvd = Statistic("mcsotdma_statistic_num_packets_received", this);
		Statistic stat_num_broadcasts_rcvd = Statistic("mcsotdma_statistic_num_broadcasts_received", this);
		Statistic stat_num_broadcast_msgs_decoded = Statistic("mcsotdma_statistic_num_broadcast_message_decoded", this);
		Statistic stat_num_unicasts_rcvd = Statistic("mcsotdma_statistic_num_unicasts_received", this);
		Statistic stat_num_unicast_msgs_decoded = Statistic("mcsotdma_statistic_num_unicast_message_decoded", this);
		Statistic stat_num_requests_rcvd = Statistic("mcsotdma_statistic_num_link_requests_received", this);
		Statistic stat_num_replies_rcvd = Statistic("mcsotdma_statistic_num_link_replies_received", this);
		Statistic stat_num_beacons_rcvd = Statistic("mcsotdma_statistic_num_beacons_received", this);
		Statistic stat_num_link_infos_rcvd = Statistic("mcsotdma_statistic_num_link_infos_received", this);
		Statistic stat_num_packets_sent = Statistic("mcsotdma_statistic_num_packets_sent", this);
		Statistic stat_num_requests_sent = Statistic("mcsotdma_statistic_num_link_requests_sent", this);
		Statistic stat_num_broadcasts_sent = Statistic("mcsotdma_statistic_num_broadcasts_sent", this);
		Statistic stat_num_unicasts_sent = Statistic("mcsotdma_statistic_num_unicasts_sent", this);
		Statistic stat_num_replies_sent = Statistic("mcsotdma_statistic_num_link_replies_sent", this);
		Statistic stat_num_beacons_sent = Statistic("mcsotdma_statistic_num_beacons_sent", this);
		Statistic stat_num_link_infos_sent = Statistic("mcsotdma_statistic_num_link_infos_sent", this);
		Statistic stat_num_requests_cancelled = Statistic("mcsotdma_statistic_num_cancelled_link_requests", this);
		Statistic stat_num_packet_collisions = Statistic("mcsotdma_statistic_num_packet_collisions", this);
		Statistic stat_num_packets_decoded = Statistic("mcsotdma_statistic_num_packet_decoded", this);
		Statistic stat_num_active_neighbors = Statistic("mcsotdma_statistic_num_active_neighbors", this);
		Statistic stat_min_beacon_offset = Statistic("mcsotdma_statistic_min_beacon_offset", this);
		Statistic stat_contention = Statistic("mcsotdma_statistic_contention", this);
		Statistic stat_congestion = Statistic("mcsotdma_statistic_congestion", this);
		Statistic stat_broadcast_candidate_slots = Statistic("mcsotdma_statistic_broadcast_candidate_slots", this);
		std::vector<Statistic*> statistics = {
				&stat_num_packets_rcvd,
				&stat_num_broadcasts_rcvd,
				&stat_num_broadcast_msgs_decoded,
				&stat_num_unicasts_rcvd,
				&stat_num_unicast_msgs_decoded,
				&stat_num_requests_rcvd,
				&stat_num_replies_rcvd,
				&stat_num_beacons_rcvd,
				&stat_num_link_infos_rcvd,
				&stat_num_packets_sent,
				&stat_num_requests_sent,
				&stat_num_broadcasts_sent,
				&stat_num_unicasts_sent,
				&stat_num_replies_sent,
				&stat_num_beacons_sent,
				&stat_num_link_infos_sent,
				&stat_num_requests_cancelled,
				&stat_num_packet_collisions,
				&stat_num_packets_decoded,
				&stat_num_active_neighbors,
				&stat_min_beacon_offset,
				&stat_contention,
				&stat_congestion,
				&stat_broadcast_candidate_slots
		};
//		const std::string str_statistic_num_packets_received = "mcsotdma_statistic_num_packets_received";
//		const std::string str_statistic_num_broadcasts_received = "mcsotdma_statistic_num_broadcasts_received";
//		const std::string str_statistic_num_broadcast_message_decoded = "mcsotdma_statistic_num_broadcast_message_decoded";
//		const std::string str_statistic_num_unicasts_received = "mcsotdma_statistic_num_unicasts_received";
//		const std::string str_statistic_num_unicast_message_decoded = "mcsotdma_statistic_num_unicast_message_decoded";
//		const std::string str_statistic_num_requests_received = "mcsotdma_statistic_num_link_requests_received";
//		const std::string str_statistic_num_replies_received = "mcsotdma_statistic_num_link_replies_received";
//		const std::string str_statistic_num_beacons_received = "mcsotdma_statistic_num_beacons_received";
//		const std::string str_statistic_num_link_infos_received = "mcsotdma_statistic_num_link_infos_received";
//		const std::string str_statistic_num_packets_sent = "mcsotdma_statistic_num_packets_sent";
//		const std::string str_statistic_num_requests_sent = "mcsotdma_statistic_num_link_requests_sent";
//		const std::string str_statistic_num_broadcasts_sent = "mcsotdma_statistic_num_broadcasts_sent";
//		const std::string str_statistic_num_unicasts_sent = "mcsotdma_statistic_num_unicasts_sent";
//		const std::string str_statistic_num_replies_sent = "mcsotdma_statistic_num_link_replies_sent";
//		const std::string str_statistic_num_beacons_sent = "mcsotdma_statistic_num_beacons_sent";
//		const std::string str_statistic_num_link_infos_sent = "mcsotdma_statistic_num_link_infos_sent";
//		const std::string str_statistic_num_cancelled_link_requests = "mcsotdma_statistic_num_cancelled_link_requests";
//		const std::string str_statistic_num_packet_collisions = "mcsotdma_statistic_num_packet_collisions";
//		const std::string str_statistic_num_packet_decoded = "mcsotdma_statistic_num_packet_decoded";
//		const std::string str_statistic_num_active_neighbors = "mcsotdma_statistic_num_active_neighbors";
//		const std::string str_statistic_min_beacon_offset = "mcsotdma_statistic_min_beacon_offset";
//		const std::string str_statistic_contention = "mcsotdma_statistic_contention";
//		const std::string str_statistic_congestion = "mcsotdma_statistic_congestion";
//		const std::string str_statistic_broadcast_candidate_slots = "mcsotdma_statistic_broadcast_candidate_slots";
//		size_t statistic_num_packets_received = 0;
//		size_t statistic_num_broadcast_message_decoded = 0;
//		size_t statistic_num_broadcasts_received = 0;
//		size_t statistic_num_unicast_message_decoded = 0;
//		size_t statistic_num_unicasts_received = 0;
//		size_t statistic_num_requests_received = 0;
//		size_t statistic_num_replies_received = 0;
//		size_t statistic_num_beacons_received = 0;
//		size_t statistic_num_link_infos_received = 0;
//		size_t statistic_num_packets_sent = 0;
//		size_t statistic_num_broadcasts_sent = 0;
//		size_t statistic_num_unicasts_sent = 0;
//		size_t statistic_num_requests_sent = 0;
//		size_t statistic_num_replies_sent = 0;
//		size_t statistic_num_beacons_sent = 0;
//		size_t statistic_num_link_infos_sent = 0;
//		size_t statistic_num_packet_collisions = 0;
//		size_t statistic_num_packet_decoded = 0;
//		size_t statistic_num_cancelled_link_requests = 0;
//		size_t statistic_num_active_neighbors = 0;
//		size_t statistic_min_beacon_offset = 0;
//		double statistic_contention = 0.0;
//		double statistic_congestion = 0.0;
//		size_t statistic_broadcast_candidate_slots = 0;
	};

	inline std::ostream& operator<<(std::ostream& stream, const MCSOTDMA_Mac& mac) {
		return stream << "MAC(" << mac.getMacId() << ")";
	}
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
