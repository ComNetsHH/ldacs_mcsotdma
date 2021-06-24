//
// Created by Sebastian Lindner on 16.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP

#include <IMac.hpp>
#include <L2Packet.hpp>
#include <IArq.hpp>
#include <IOmnetPluggable.hpp>
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

		void statisticReportLinkRequestReceived() {
			statistic_num_requests_received++;
		}
		void statisticReportLinkReplyReceived() {
			statistic_num_replies_received++;
		}
		void statisticReportBeaconReceived() {
			statistic_num_beacons_received++;
		}
		void statisticReportLinkInfoReceived() {
			statistic_num_link_infos_received++;
		}
		void statisticReportPacketSent() {
			statistic_num_packets_sent++;
		}
		void statisticReportLinkRequestSent() {
			statistic_num_requests_sent++;
		}
		void statisticReportLinkReplySent() {
			statistic_num_replies_sent++;
		}
		void statisticReportBeaconSent() {
			statistic_num_beacons_sent++;
		}
		void statisticReportLinkInfoSent() {
			statistic_num_link_infos_sent++;
		}
		void statisticReportCancelledLinkRequest() {
			statistic_num_cancelled_link_requests++;
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
		const std::string str_statistic_num_packets_received = "MCSOTDMA:statistic_num_packets_received(num)";
		const std::string str_statistic_num_requests_received = "MCSOTDMA:statistic_num_link_requests_received(num)";
		const std::string str_statistic_num_replies_received = "MCSOTDMA:statistic_num_link_replies_received(num)";
		const std::string str_statistic_num_beacons_received = "MCSOTDMA:statistic_num_beacons_received(num)";
		const std::string str_statistic_num_link_infos_received = "MCSOTDMA:statistic_num_link_infos_received(num)";
		const std::string str_statistic_num_packets_sent = "MCSOTDMA:statistic_num_packets_sent(num)";
		const std::string str_statistic_num_requests_sent = "MCSOTDMA:statistic_num_link_requests_sent(num)";
		const std::string str_statistic_num_replies_sent = "MCSOTDMA:statistic_num_link_replies_sent(num)";
		const std::string str_statistic_num_beacons_sent = "MCSOTDMA:statistic_num_beacons_sent(num)";
		const std::string str_statistic_num_link_infos_sent = "MCSOTDMA:statistic_num_link_infos_sent(num)";
		const std::string str_statistic_num_packet_collisions = "MCSOTDMA:statistic_num_packet_collisions(num)";
		const std::string str_statistic_num_packet_decoded = "MCSOTDMA:statistic_num_packet_decoded(num)";
		const std::string str_statistic_num_cancelled_link_requests = "MCSOTDMA:statistic_num_cancelled_link_requests(num)";
		size_t statistic_num_packets_received = 0;
		size_t statistic_num_requests_received = 0;
		size_t statistic_num_replies_received = 0;
		size_t statistic_num_beacons_received = 0;
		size_t statistic_num_link_infos_received = 0;
		size_t statistic_num_packets_sent = 0;
		size_t statistic_num_requests_sent = 0;
		size_t statistic_num_replies_sent = 0;
		size_t statistic_num_beacons_sent = 0;
		size_t statistic_num_link_infos_sent = 0;
		size_t statistic_num_packet_collisions = 0;
		size_t statistic_num_packet_decoded = 0;
		size_t statistic_num_cancelled_link_requests = 0;
	};

	inline std::ostream& operator<<(std::ostream& stream, const MCSOTDMA_Mac& mac) {
		return stream << "MAC(" << mac.getMacId() << ")";
	}
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
