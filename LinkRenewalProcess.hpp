//
// Created by seba on 1/14/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKRENEWALPROCESS_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKRENEWALPROCESS_HPP

#include "LinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

    /**
     * LinkManager module that handles the P2P link renewal protocol.
     */
    class LinkRenewalProcess {

        friend class LinkRenewalProcessTests;

    public:
        explicit LinkRenewalProcess(LinkManager* owner);

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
        bool update();

    protected:
        std::vector<uint64_t> scheduleRequests(unsigned int tx_timeout, unsigned int init_offset, unsigned int tx_offset) const;

    protected:
        /** Number of times a link should still be attempted to be renewed. */
        unsigned int remaining_attempts = 0;
        /** A LinkRenewalProcess is a module of a LinkManager. */
        LinkManager* owner = nullptr;
        /** The absolute points in time when requests should be sent. */
        std::vector<uint64_t> absolute_request_slots;
    };
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKRENEWALPROCESS_HPP
