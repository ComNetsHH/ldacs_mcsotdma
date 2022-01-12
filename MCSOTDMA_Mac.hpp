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
#include <ContentionMethod.hpp>
#include "ReservationManager.hpp"
#include "LinkManager.hpp"
#include "MCSOTDMA_Phy.hpp"
#include "NeighborObserver.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * Implements the MAC interface.
	 */
	class MCSOTDMA_Mac : public IMac, public IOmnetPluggable {
	public:
		friend class MCSOTDMA_MacTests;
		friend class MCSOTDMA_PhyTests;
		friend class ThreeUsersTests;
		friend class P2PLinkManagerTests;
		friend class NewPPLinkManagerTests;
		friend class SHLinkManagerTests;

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

		void setBcSlotSelectionMinNumCandidateSlots(int value) override;
		void setBcSlotSelectionMaxNumCandidateSlots(int value) override;

		void setContentionMethod(ContentionMethod method) override;

		void setAlwaysScheduleNextBroadcastSlot(bool value) override;
		void setAdvertiseNextBroadcastSlotInCurrentHeader(bool flag) override;

		void setCloseP2PLinksEarly(bool flag) override;

		void setMinBeaconOffset(unsigned int value) override;
		void setMaxBeaconOffset(unsigned int value) override;

		void setForceBidirectionalLinks(bool flag) override;
		void setInitializeBidirectionalLinks(bool flag) override;

		void setWriteResourceUtilizationIntoBeacon(bool flag) override;
		void setEnableBeacons(bool flag) override;

		size_t getNumUtilizedP2PResources() const;

		/** Link managers call this to report broadcast or unicast activity from a neighbor. This is used to update the recently active neighbors. */
		void reportNeighborActivity(const MacId& id);
		const NeighborObserver& getNeighborObserver() const;

		void statisticReportBroadcastMessageProcessed() {
			stat_num_broadcast_msgs_processed.increment();
		}
		void statisticReportBroadcastReceived() {
			stat_num_broadcasts_rcvd.increment();
		}
		void statisticReportUnicastMessageProcessed() {
			stat_num_unicast_msgs_processed.increment();
		}
		void statisticReportUnicastReceived() {
			stat_num_unicasts_rcvd.increment();
		}
		void statisticReportLinkRequestReceived() {
			stat_num_requests_rcvd.increment();
		}
		void statisticReportThirdPartyLinkRequestReceived() {
			stat_num_thid_party_requests_rcvd.increment();
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
		void statisticReportSelectedBroadcastCandidateSlots(size_t val) {
			stat_broadcast_selected_candidate_slots.capture(val);
		}
		/** My link is established after I've sent my link reply and receive the first data packet. If that doesn't arrive within as many attempts as ARQ allows, I should close the link early. This counts the number of times this has happened. */
		void statisticReportLinkClosedEarly() {
			stat_num_links_closed_early.increment();
		}
		/**
		 * @param mac_delay: In slots.
		 */
		void statisticReportBroadcastMacDelay(unsigned int mac_delay) {			
			stat_broadcast_mac_delay.capture((double) mac_delay);
		}

		void statistcReportPPLinkMissedLastReplyOpportunity() {
			stat_pp_link_missed_last_reply_opportunity.increment();
		}

		/** After sending a reply we wait for the first data transmission. If this doesn't arrive, this statistic counts the event. */
		void statistcReportPPLinkMissedFirstDataTx() {
			stat_pp_link_missed_first_data_tx.increment();
		}		

		/**
		 * @param num_slots Number of slots that have passed since the sending of a link request, and the final establishment of the link.
		 */
		void statisticReportPPLinkEstablishmentTime(unsigned int num_slots) {
			stat_pp_link_establishment_time.capture(num_slots);
		}

		void statisticReportPPLinkEstablished() {
			stat_num_pp_links_established.increment();
		}

		void statisticReportPPLinkExpired() {
			stat_num_pp_links_expired.increment();
		}

		void statisticReportLinkRequestRejectedDueToUnacceptableReplySlot() {
			stat_num_pp_requests_rejected_due_to_unacceptable_reply_slot.increment();
		}

		void statisticReportLinkRequestRejectedDueToUnacceptablePPResourceProposals() {
			stat_num_pp_requests_rejected_due_to_unacceptable_pp_resource_proposals.increment();
		}

		unsigned int getP2PBurstOffset() const;

		bool isUsingNewPPLinkManager() const;

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
		/** My link is established after I've sent my link reply and receive the first data packet. If that doesn't arrive within as many attempts as ARQ allows, I should close the link early if this flag is set. */
		bool close_link_early_if_no_first_data_packet_comes_in = false;		
		/** Keeps a list of active neighbors, which have demonstrated activity within the last 50.000 slots (10min if slot duration is 12ms). */
		NeighborObserver active_neighbor_observer; 
		/** Number of transmission bursts before a P2P link expires. */
		const unsigned int default_p2p_link_timeout = 10;
		/** Number of slots between two transmission bursts. */
		const unsigned int default_p2p_link_burst_offset = 20;
		/** TODO REMOVE; while working on it, this flag creates NewPPLinkManager instances. */
		bool use_new_pp_link_manager = false;

		// Statistics
		Statistic stat_num_packets_rcvd = Statistic("mcsotdma_statistic_num_packets_received", this);
		Statistic stat_num_broadcasts_rcvd = Statistic("mcsotdma_statistic_num_broadcasts_received", this);
		Statistic stat_num_broadcast_msgs_processed = Statistic("mcsotdma_statistic_num_broadcast_message_processed", this);
		Statistic stat_num_unicasts_rcvd = Statistic("mcsotdma_statistic_num_unicasts_received", this);
		Statistic stat_num_unicast_msgs_processed = Statistic("mcsotdma_statistic_num_unicast_message_processed", this);
		Statistic stat_num_requests_rcvd = Statistic("mcsotdma_statistic_num_link_requests_received", this);
		Statistic stat_num_thid_party_requests_rcvd = Statistic("mcsotdma_statistic_num_third_party_link_requests_received", this);
		Statistic stat_num_replies_rcvd = Statistic("mcsotdma_statistic_num_link_replies_received", this);
		Statistic stat_num_beacons_rcvd = Statistic("mcsotdma_statistic_num_beacons_received", this);
		Statistic stat_num_link_infos_rcvd = Statistic("mcsotdma_statistic_num_link_infos_received", this);
		Statistic stat_num_packets_sent = Statistic("mcsotdma_statistic_num_packets_sent", this);
		Statistic stat_num_requests_sent = Statistic("mcsotdma_statistic_num_link_requests_sent", this);
		/** Number of non-beacon broadcast-type packets that were sent. */
		Statistic stat_num_broadcasts_sent = Statistic("mcsotdma_statistic_num_broadcasts_sent", this);
		Statistic stat_num_unicasts_sent = Statistic("mcsotdma_statistic_num_unicasts_sent", this);
		Statistic stat_num_replies_sent = Statistic("mcsotdma_statistic_num_link_replies_sent", this);
		Statistic stat_num_beacons_sent = Statistic("mcsotdma_statistic_num_beacons_sent", this);
		Statistic stat_num_link_infos_sent = Statistic("mcsotdma_statistic_num_link_infos_sent", this);
		Statistic stat_num_requests_cancelled = Statistic("mcsotdma_statistic_num_cancelled_link_requests", this);
		Statistic stat_num_packet_collisions = Statistic("mcsotdma_statistic_num_packet_collisions", this);		
		Statistic stat_num_channel_errors = Statistic("mcsotdma_statistic_num_channel_errors", this);		
		Statistic stat_num_active_neighbors = Statistic("mcsotdma_statistic_num_active_neighbors", this);
		Statistic stat_min_beacon_offset = Statistic("mcsotdma_statistic_min_beacon_offset", this);
		Statistic stat_contention = Statistic("mcsotdma_statistic_contention", this);
		Statistic stat_congestion = Statistic("mcsotdma_statistic_congestion", this);
		Statistic stat_broadcast_candidate_slots = Statistic("mcsotdma_statistic_broadcast_candidate_slots", this);
		Statistic stat_broadcast_selected_candidate_slots = Statistic("mcsotdma_statistic_broadcast_selected_candidate_slot", this);
		Statistic stat_num_links_closed_early = Statistic("mcsotdma_statistic_num_links_closed_early", this);
		Statistic stat_broadcast_mac_delay = Statistic("mcsotdma_statistic_broadcast_mac_delay", this);				
		Statistic stat_broadcast_wasted_tx_opportunities = Statistic("mcsotdma_statistic_broadcast_wasted_tx_opportunities", this);
		Statistic stat_unicast_wasted_tx_opportunities = Statistic("mcsotdma_statistic_unicast_wasted_tx_opportunities", this);
		Statistic stat_pp_link_missed_last_reply_opportunity = Statistic("mcsotdma_statistic_pp_link_missed_last_reply_opportunity", this);
		Statistic stat_pp_link_missed_first_data_tx = Statistic("mcsotdma_statistic_pp_link_missed_first_data_tx", this);
		Statistic stat_pp_link_establishment_time = Statistic("mcsotdma_statistic_pp_link_establishment_time", this);		
		Statistic stat_num_pp_links_established = Statistic("mcsotdma_statistic_num_pp_links_established", this);
		Statistic stat_num_pp_links_expired = Statistic("mcsotdma_statistic_num_pp_links_expired", this);
		Statistic stat_num_pp_requests_rejected_due_to_unacceptable_reply_slot = Statistic("num_pp_requests_rejected_due_to_unacceptable_reply_slot", this);
		Statistic stat_num_pp_requests_rejected_due_to_unacceptable_pp_resource_proposals = Statistic("num_pp_requests_rejected_due_to_unacceptable_pp_resource_proposals", this);		
		std::vector<Statistic*> statistics = {
				&stat_num_packets_rcvd,
				&stat_num_broadcasts_rcvd,
				&stat_num_broadcast_msgs_processed,
				&stat_num_unicasts_rcvd,
				&stat_num_unicast_msgs_processed,
				&stat_num_requests_rcvd,
				&stat_num_thid_party_requests_rcvd,
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
				&stat_num_channel_errors,			
				&stat_num_active_neighbors,
				&stat_min_beacon_offset,
				&stat_contention,
				&stat_congestion,
				&stat_broadcast_candidate_slots,
				&stat_broadcast_selected_candidate_slots,
				&stat_num_links_closed_early,
				&stat_broadcast_mac_delay,
				&stat_broadcast_wasted_tx_opportunities,
				&stat_unicast_wasted_tx_opportunities,
				&stat_pp_link_missed_last_reply_opportunity,
				&stat_pp_link_missed_first_data_tx,
				&stat_pp_link_establishment_time,
				&stat_num_pp_links_established,
				&stat_num_pp_links_expired,
				&stat_num_pp_requests_rejected_due_to_unacceptable_reply_slot,
				&stat_num_pp_requests_rejected_due_to_unacceptable_pp_resource_proposals
		};
	};

	inline std::ostream& operator<<(std::ostream& stream, const MCSOTDMA_Mac& mac) {
		return stream << "MAC(" << mac.getMacId() << ")";
	}
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
