//
// Created by seba on 1/14/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGEMENTPROCESS_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGEMENTPROCESS_HPP

#include "LinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

    /**
     * LinkManager module that handles the P2P link management, such as processing requests and replies.
     */
    class LinkManagementProcess {

        friend class LinkRenewalProcessTests;
        friend class LinkManagerTests;

    public:
        explicit LinkManagementProcess(LinkManager* owner);

        /**
         * When a new reservation is established, this resets the process and starts it anew.
         * @param num_renewal_attempts
         * @param tx_timeout
         * @param init_offset
         * @param tx_offset
         */
        void configure(unsigned int num_renewal_attempts, unsigned int tx_timeout, unsigned int init_offset, unsigned int tx_offset);

        /**
         * @return Whether a link request should be sent.
         */
        bool shouldSendRequest();

        /**
         * When a LinkManager receives a link reply, it should forward it to this function.
         * @param header
         * @param payload
         */
        void processLinkReply(const L2HeaderLinkEstablishmentReply*& header, const LinkManager::ProposalPayload*& payload);

        /**
         * When a LinkManager receoves a link request, it should forward it to this function.
         * @param header
         * @param payload
         */
        void processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header, const LinkManager::ProposalPayload*& payload, const MacId& origin);

        void onTransmissionSlot();

    protected:
        std::vector<uint64_t> scheduleRequests(unsigned int tx_timeout, unsigned int init_offset, unsigned int tx_offset) const;

        std::vector<std::pair<const FrequencyChannel*, unsigned int>> findViableCandidatesInRequest(L2HeaderLinkEstablishmentRequest*& header, LinkManager::ProposalPayload*& payload) const;

    protected:
        /** Number of times a link should be attempted to be renewed. */
        unsigned int num_renewal_attempts = 0;
        /** A LinkManagementProcess is a module of a LinkManager. */
        LinkManager* owner = nullptr;
        /** The absolute points in time when requests should be sent. */
        std::vector<uint64_t> absolute_request_slots;
    };
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGEMENTPROCESS_HPP
