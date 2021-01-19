//
// Created by seba on 1/14/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGEMENTENTITY_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGEMENTENTITY_HPP

#include <cmath>
#include <map>
#include "L2Packet.hpp"
#include "FrequencyChannel.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

    class LinkManager;

    /**
     * LinkManager module that handles the P2P link management, such as processing requests and replies.
     */
    class LinkManagementEntity {

        friend class LinkManagementProcessTests;
        friend class LinkManagerTests;
        friend class SystemTests;

    public:

        /**
         * Implements a link establishment payload that encodes proposed frequency channels and slots.
         * Link requests may contain a number of channels and slots, while replies should contain just a single one.
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


        explicit LinkManagementEntity(LinkManager* owner);

        /**
         * When a new reservation is established, this resets the process and starts it anew.
         * @param num_renewal_attempts
         * @param tx_timeout
         * @param init_offset
         * @param tx_offset
         */
        void configure(unsigned int num_renewal_attempts, unsigned int tx_timeout, unsigned int init_offset, unsigned int tx_offset);

        /**
         * @return Whether a link management control message should be sent.
         */
        bool hasControlMessage();

        L2Packet* getControlMessage();

        /**
         * When a LinkManager receives a link reply, it should forward it to this function.
         * @param header
         * @param payload
         */
        void processLinkReply(const L2HeaderLinkEstablishmentReply*& header, const ProposalPayload*& payload);

        /**
         * When a LinkManager receoves a link request, it should forward it to this function.
         * @param header
         * @param payload
         */
        void processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header, const ProposalPayload*& payload, const MacId& origin);

        void onTransmissionBurst();

        /**
         * Prepares a link request and injects it into the upper layers.
         */
        void establishLink() const;

        void scheduleLinkReply(L2Packet* reply, int32_t slot_offset, unsigned int timeout, unsigned int offset, unsigned int length);

        void populateRequest(L2Packet*& request);

        /**
         * @param value Number of transmission bursts a reservation should be valid for.
         */
        void setTxTimeout(unsigned int value);

        /**
         * @param value Number of slots inbetween two transmission bursts.
         */
        void setTxOffset(unsigned int value);

        /**
         * @return Number of transmission bursts the reservation is still valid for.
         */
        unsigned int getTxTimeout() const;

        /**
         * @return Number of slots in-between two transmission bursts.
         */
        unsigned int getTxOffset() const;

        /**
         * @return Minimum offset for new reservations.
         */
        unsigned int getMinOffset() const;

        /**
         * @return Number of consecutive slots used per transmission burst.
         */
        unsigned int getTxBurstSlots() const;

        void onRequestTransmission();

    protected:
        std::vector<uint64_t> scheduleRequests(unsigned int tx_timeout, unsigned int init_offset, unsigned int tx_offset) const;

        std::vector<std::pair<const FrequencyChannel*, unsigned int>> findViableCandidatesInRequest(L2HeaderLinkEstablishmentRequest*& header, ProposalPayload*& payload) const;

        L2Packet* prepareRequest() const;

        L2Packet* prepareReply(const MacId& destination_id) const;

        bool hasPendingRequest();

        bool hasPendingReply();

        /**
         * @return A payload that should accompany a link request.
         */
        ProposalPayload* p2pSlotSelection();

    protected:
        /** Number of times a link should be attempted to be renewed. */
        unsigned int num_renewal_attempts = 0;
        /** A LinkManagementEntity is a module of a LinkManager. */
        LinkManager* owner = nullptr;
        /** The absolute points in time when requests should be sent. */
        std::vector<uint64_t> scheduled_requests;
        /** Link replies *must* be sent on specific slots. This container holds these bindings. */
        std::map<uint64_t , L2Packet*> scheduled_replies;
        /** Number of attempts to renew a link before giving up. */
        unsigned int link_renewal_attempts = 3;
        /** The minimum number of slots a proposed slot should be in the future. */
        const int32_t minimum_slot_offset_for_new_slot_reservations = 1;
        /** The number of frequency channels that should be proposed when a new link request is prepared. */
        unsigned int num_proposed_channels = 2;
        /** The number of time slots that should be proposed when a new link request is prepared. */
        unsigned int num_proposed_slots = 3;
        /** Number of repetitions a reservation remains valid for. */
        unsigned int default_tx_timeout = 10,
                tx_timeout = default_tx_timeout;
        /** Number of slots occupied per transmission burst. */
        unsigned int tx_burst_num_slots = 1;
        /** Number of slots until the next transmission. Should be set to the P2P frame length, or dynamically for broadcast-type transmissions. */
        unsigned int tx_offset = 5;
    };
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGEMENTENTITY_HPP
