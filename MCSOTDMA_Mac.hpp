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
#include "ThirdPartyLink.hpp"
#include "DutyCycle.hpp"


namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * Implements the MAC interface.
	 */
	class MCSOTDMA_Mac : public IMac, public IOmnetPluggable {
	public:
		friend class MCSOTDMA_MacTests;
		friend class MCSOTDMA_PhyTests;
		friend class ManyUsersTests;		
		friend class PPLinkManagerTests;
		friend class SHLinkManagerTests;
		friend class ThirdPartyLinkTests;

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

		ThirdPartyLink& getThirdPartyLink(const MacId& id1, const MacId& id2);

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
		void setAdvertiseNextBroadcastSlotInCurrentHeader(bool flag) override;				
		void setMaxNoPPLinkEstablishmentAttempts(int value) override;
		void setMinNumSupportedPPLinks(unsigned int value) override;
		void setForcePPPeriod(bool flag, int value) override;

		void setDutyCycleBudgetComputationStrategy(const DutyCycleBudgetStrategy& strategy) override;

		size_t getNumUtilizedP2PResources() const;
		std::vector<L2HeaderSH::LinkUtilizationMessage> getPPLinkUtilizations() const;
		const std::map<MacId, LinkManager*>& getLinkManagers() const;


		void onThirdPartyLinkReset(const ThirdPartyLink* caller);

		virtual bool isGoingToTransmitDuringCurrentSlot(uint64_t center_frequency) const;

		/** Link managers call this to report broadcast or unicast activity from a neighbor. This is used to update the recently active neighbors. */
		void reportNeighborActivity(const MacId& id);
		void reportBroadcastSlotAdvertisement(const MacId& id, unsigned int advertised_slot_offset);
		NeighborObserver& getNeighborObserver();		

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
			stat_num_third_party_requests_rcvd.increment();
		}		
		void statisticReportThirdPartyLinkReplyReceived() {
			stat_num_third_party_replies_rcvd.increment();
		}
		void statisticReportLinkReplyReceived() {
			stat_num_replies_rcvd.increment();
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
		void statisticReportNumActiveNeighbors(size_t val) {
			stat_num_active_neighbors.capture(val);
		}				
		void statisticReportBroadcastCandidateSlots(size_t val) {
			stat_broadcast_candidate_slots.capture(val);
		}
		void statisticReportSelectedBroadcastCandidateSlots(size_t val) {
			stat_broadcast_selected_candidate_slots.capture(val);
		}		
		/**
		 * @param mac_delay: In slots.
		 */
		void statisticReportBroadcastMacDelay(unsigned int mac_delay) {			
			stat_broadcast_mac_delay.capture((double) mac_delay);
		}
		void statisticReportAvgBeaconReceptionDelay(double avg_delay) {
			stat_avg_beacon_rx_delay.capture(avg_delay);
		}
		void statisticReportFirstNeighborAvgBeaconReceptionDelay(double avg_delay) {
			stat_first_neighbor_beacon_rx_delay.capture(avg_delay);
		}

		void statisticReportUnicastMacDelay(unsigned int mac_delay) {			
			stat_unicast_mac_delay.capture((double) mac_delay);
		}		

		void statisticReportPPLinkMissedLastReplyOpportunity() {
			stat_pp_link_missed_last_reply_opportunity.increment();
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

		void statisticReportBroadcastCollisionDetected() {
			stat_num_broadcast_collisions_detected.increment();
		}				
		void statisticReportSentOwnProposals() {
			stat_num_own_proposals_sent.increment();
		}
		void statisticReportSentSavedProposals() {
			stat_num_saved_proposals_sent.increment();
		}		
		void statisticReportLinkUtilizationReceived() {
			stat_num_link_utils_rcvd.increment();
		}				
		void statisticReportLinkRequestAccepted() {
			stat_num_pp_link_requests_accepted.increment();
		}				
		void statisticReportMaxNoOfPPLinkEstablishmentAttemptsExceeded() {
			stat_pp_link_exceeded_max_no_establishment_attempts.increment();
		}				
		void statisticReportPPPeriodUsed(double value) {
			stat_pp_period.capture(value);
		}
						

		unsigned int getP2PBurstOffset() const;		

		void setDutyCycle(unsigned int period, double max, unsigned int min_num_supported_pp_links) override;
		const DutyCycle& getDutyCycle() const;
		void setConsiderDutyCycle(bool flag) override;
		bool shouldConsiderDutyCycle() const;

		bool shouldLearnDmeActivity() const;

		const std::vector<int> getChannelSensingObservation() const override;
		void setLearnDMEActivity(bool value) override;
		void passPrediction(const std::vector<std::vector<double>>& prediction_mat) override;
		std::vector<std::vector<double>>& getCurrentPrediction();
		virtual void notifyAboutDmeTransmission(uint64_t center_frequency) override;

		/**		 
		 * @return <used budget per link, timeout per link>
		 */
		std::pair<std::vector<double>, std::vector<int>> getUsedPPDutyCycleBudget() const;		
		double getUsedSHDutyCycleBudget() const;		
		int getSHSlotOffset() const; 
		int getDefaultPPLinkTimeout() const;
		bool shouldUseFixedPPPeriod() const;
		int getFixedPPPeriod() const;
		size_t getNumActivePPLinks() const;		

	protected:
		/**
		 * Define what happens when a particular FrequencyChannel should be listened on during this time slot.
		 * @param channel
		 */
		void onReceptionSlot(const FrequencyChannel* channel);
		void storePacket(L2Packet *&packet, uint64_t center_freq);

		/** Keeps track of transmission resource reservations. */
		ReservationManager* reservation_manager;
		/** Maps links to their link managers. */
		std::map<MacId, LinkManager*> link_managers;
		std::map<std::pair<MacId, MacId>, ThirdPartyLink> third_party_links;
		const size_t num_transmitters = 1, num_receivers = 2;
		/** Holds the current belief of neighbor positions. */
		std::map<MacId, CPRPosition> position_map;
		std::map<uint64_t, std::vector<L2Packet*>> received_packets;		
		/** Keeps a list of active neighbors, which have demonstrated activity within the last 50.000 slots (10min if slot duration is 12ms). */
		NeighborObserver neighbor_observer; 
		/** Number of transmission bursts before a P2P link expires. */
		const unsigned int default_p2p_link_timeout = 10;
		/** Number of slots between two transmission bursts. */
		const unsigned int default_p2p_link_burst_offset = 20;		
		unsigned int pp_link_burst_offset = 20;
		bool adapt_burst_offset = true;
		bool use_duty_cycle = true;
		int default_pp_link_timeout = 20;
				
		/** Percentage of time that can be used for transmissions. */
		const double default_max_duty_cycle = 0.1;
		/** Number of time slots to consider when checking whether the duty cycle is kept. */
		const unsigned int default_duty_cycle_period = 100;
		/** Number of PP links that the MAC should be able to maintain, given the maximum duty cycle. */
		const unsigned int default_min_num_supported_pp_links = 1;
		DutyCycle duty_cycle;

		bool learn_dme_activity = false;
		std::map<uint64_t, bool> channel_sensing_observation;	
		/** Holds the current prediction of DME channel accesses. */
		std::vector<std::vector<double>> current_prediction_mat;	
		int max_no_pp_link_establishment_attempts = 5;
		/** If false, the PP period is computed during link establishment. If true, a fixed value is used. */
		bool should_force_pp_period = false;
		/** Is used for each PP link if should_force_pp_period is true. */
		int forced_pp_period = 1;

		// Statistics
		Statistic stat_num_packets_rcvd = Statistic("mcsotdma_statistic_num_packets_received", this);
		Statistic stat_num_broadcasts_rcvd = Statistic("mcsotdma_statistic_num_broadcasts_received", this);
		Statistic stat_num_broadcast_msgs_processed = Statistic("mcsotdma_statistic_num_broadcast_message_processed", this);
		Statistic stat_num_unicasts_rcvd = Statistic("mcsotdma_statistic_num_unicasts_received", this);
		Statistic stat_num_unicast_msgs_processed = Statistic("mcsotdma_statistic_num_unicast_message_processed", this);
		Statistic stat_num_requests_rcvd = Statistic("mcsotdma_statistic_num_link_requests_received", this);
		Statistic stat_num_third_party_requests_rcvd = Statistic("mcsotdma_statistic_num_third_party_link_requests_received", this);
		Statistic stat_num_replies_rcvd = Statistic("mcsotdma_statistic_num_link_replies_received", this);
		Statistic stat_num_third_party_replies_rcvd = Statistic("mcsotdma_statistic_num_third_party_replies_rcvd", this);		
		Statistic stat_num_packets_sent = Statistic("mcsotdma_statistic_num_packets_sent", this);
		Statistic stat_num_requests_sent = Statistic("mcsotdma_statistic_num_link_requests_sent", this);		
		Statistic stat_num_broadcasts_sent = Statistic("mcsotdma_statistic_num_broadcasts_sent", this);
		Statistic stat_num_unicasts_sent = Statistic("mcsotdma_statistic_num_unicasts_sent", this);
		Statistic stat_num_replies_sent = Statistic("mcsotdma_statistic_num_link_replies_sent", this);				
		Statistic stat_num_packet_collisions = Statistic("mcsotdma_statistic_num_packet_collisions", this);				
		Statistic stat_num_channel_errors = Statistic("mcsotdma_statistic_num_channel_errors", this);		
		Statistic stat_num_active_neighbors = Statistic("mcsotdma_statistic_num_active_neighbors", this);		
		Statistic stat_broadcast_candidate_slots = Statistic("mcsotdma_statistic_broadcast_candidate_slots", this);
		Statistic stat_broadcast_selected_candidate_slots = Statistic("mcsotdma_statistic_broadcast_selected_candidate_slot", this);		
		Statistic stat_broadcast_mac_delay = Statistic("mcsotdma_statistic_broadcast_mac_delay", this);
		Statistic stat_avg_beacon_rx_delay = Statistic("mcsotdma_statistic_avg_beacon_rx_delay", this);
		Statistic stat_first_neighbor_beacon_rx_delay = Statistic("mcsotdma_statistic_first_neighbor_beacon_rx_delay", this);
		Statistic stat_unicast_mac_delay = Statistic("mcsotdma_statistic_unicast_mac_delay", this);								
		Statistic stat_pp_link_missed_last_reply_opportunity = Statistic("mcsotdma_statistic_pp_link_missed_last_reply_opportunity", this);		
		Statistic stat_pp_link_exceeded_max_no_establishment_attempts = Statistic("mcsotdma_statistic_pp_link_exceeded_max_no_establishment_attempts", this);
		Statistic stat_pp_link_establishment_time = Statistic("mcsotdma_statistic_pp_link_establishment_time", this);						
		Statistic stat_num_pp_links_established = Statistic("mcsotdma_statistic_num_pp_links_established", this);
		Statistic stat_num_pp_link_requests_accepted = Statistic("mcsotdma_statistic_pp_link_requests_accepted", this);
		Statistic stat_num_pp_links_expired = Statistic("mcsotdma_statistic_num_pp_links_expired", this);		
		/** If proposed resources are earlier than the next SH transmission. */
		Statistic stat_num_pp_requests_rejected_due_to_unacceptable_reply_slot = Statistic("mcsotdma_statistic_num_pp_requests_rejected_due_to_unacceptable_reply_slot", this);
		Statistic stat_num_pp_requests_rejected_due_to_unacceptable_pp_resource_proposals = Statistic("mcsotdma_statistic_num_pp_requests_rejected_due_to_unacceptable_pp_resource_proposals", this);				
		Statistic stat_pp_period = Statistic("mcsotdma_statistic_pp_period", this);				
		Statistic stat_num_dme_packets_rcvd = Statistic("mcsotdma_statistic_num_num_dme_packets_rcvd", this);		
		Statistic stat_num_broadcast_collisions_detected = Statistic("mcsotdma_statistic_num_broadcast_collisions_detected", this);				
		Statistic stat_duty_cycle = Statistic("mcsotdma_statistic_duty_cycle", this);		
		Statistic stat_num_own_proposals_sent = Statistic("mcsotdma_statistic_num_own_proposals_sent", this);		
		Statistic stat_num_saved_proposals_sent = Statistic("mcsotdma_statistic_num_saved_proposals_sent", this);		
		Statistic stat_num_link_utils_rcvd = Statistic("mcsotdma_statistic_num_link_utils_rcvd", this);		
		std::vector<Statistic*> statistics = {
				&stat_num_packets_rcvd,
				&stat_num_broadcasts_rcvd,
				&stat_num_broadcast_msgs_processed,
				&stat_num_unicasts_rcvd,
				&stat_num_unicast_msgs_processed,
				&stat_num_requests_rcvd,
				&stat_num_third_party_requests_rcvd,
				&stat_num_replies_rcvd,
				&stat_num_third_party_replies_rcvd,				
				&stat_num_packets_sent,
				&stat_num_requests_sent,
				&stat_num_broadcasts_sent,
				&stat_num_unicasts_sent,
				&stat_num_replies_sent,								
				&stat_num_packet_collisions,					
				&stat_num_channel_errors,			
				&stat_num_active_neighbors,				
				&stat_broadcast_candidate_slots,
				&stat_broadcast_selected_candidate_slots,				
				&stat_broadcast_mac_delay,
				&stat_avg_beacon_rx_delay,
				&stat_first_neighbor_beacon_rx_delay,
				&stat_unicast_mac_delay,				
				&stat_pp_link_missed_last_reply_opportunity,				
				&stat_pp_link_establishment_time,				
				&stat_num_pp_links_established,
				&stat_num_pp_link_requests_accepted,
				&stat_num_pp_links_expired,				
				&stat_num_pp_requests_rejected_due_to_unacceptable_reply_slot,
				&stat_num_pp_requests_rejected_due_to_unacceptable_pp_resource_proposals,				
				&stat_pp_period,
				&stat_num_dme_packets_rcvd,
				&stat_num_broadcast_collisions_detected,				
				&stat_num_link_utils_rcvd
		};
	};

	inline std::ostream& operator<<(std::ostream& stream, const MCSOTDMA_Mac& mac) {
		return stream << "MAC(" << mac.getMacId() << ")";
	}
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
