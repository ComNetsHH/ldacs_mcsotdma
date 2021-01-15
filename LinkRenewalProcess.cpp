//
// Created by seba on 1/14/21.
//

#include "LinkRenewalProcess.hpp"
#include "MCSOTDMA_Mac.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkRenewalProcess::LinkRenewalProcess(LinkManager *owner) : owner(owner) {}

void LinkRenewalProcess::configure(unsigned int num_renewal_attempts, unsigned int tx_timeout, unsigned int init_offset,
                                   unsigned int tx_offset) {
    this->num_renewal_attempts = num_renewal_attempts;
    // Schedule the absolute slots for sending requests.
    relative_request_slots = scheduleRequests(tx_timeout, init_offset, tx_offset);
}

std::vector<uint64_t> LinkRenewalProcess::scheduleRequests(unsigned int tx_timeout, unsigned int init_offset,
                                                       unsigned int tx_offset) const {
    std::vector<uint64_t> slots;
    // For each transmission burst from last to first according to this reservation...
    for (long i = 0, offset = init_offset + (tx_timeout-1)*tx_offset; slots.size() < num_renewal_attempts && offset >= init_offset; offset -= tx_offset, i++) {
        // ... add every second burst
        if (i % 2 == 1)
            slots.push_back(offset);
    }
    return slots;
}

bool LinkRenewalProcess::update(int64_t num_slots) {
    // Check if the current slot is one during which a request should be sent.
    bool should_send_request = false;
    for (auto it = relative_request_slots.begin(); it != relative_request_slots.end(); it++) {
        // Update relative offsets...
        uint64_t current_offset = *it;
        if (num_slots > current_offset) {// these are unsigned, so can't subtract first (would lead to overflow)
            std::cout << "num_slots=" << num_slots << " offset=" << current_offset << std::endl;
            throw std::invalid_argument(
                    "LinkRenewalProcess::update exceeds a scheduled request. Should've updated earlier.");
        }
        current_offset -= num_slots;
        *it = current_offset;
        // ... check if a request should now be sent.
        if (current_offset == 0) {
            relative_request_slots.erase(it);
            it--; // Update iterator as the vector has shrunk.
            if (owner->mac->isThereMoreData(owner->getLinkId()))
                should_send_request = true;
        }
    }
    return should_send_request;
}