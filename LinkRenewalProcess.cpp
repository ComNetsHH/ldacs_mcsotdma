//
// Created by seba on 1/14/21.
//

#include "LinkRenewalProcess.hpp"
#include "MCSOTDMA_Mac.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkRenewalProcess::LinkRenewalProcess(LinkManager *owner) : owner(owner) {}

void LinkRenewalProcess::configure(unsigned int num_renewal_attempts, unsigned int tx_timeout, unsigned int init_offset,
                                   unsigned int tx_offset) {
    remaining_attempts = num_renewal_attempts;
    // Schedule the absolute slots for sending requests.
    absolute_request_slots = scheduleRequests(tx_timeout, init_offset, tx_offset);
}

std::vector<uint64_t> LinkRenewalProcess::scheduleRequests(unsigned int tx_timeout, unsigned int init_offset,
                                                       unsigned int tx_offset) const {
    std::vector<uint64_t> slots;
    // For each attempt...
    for (size_t i = remaining_attempts; i > 0; i--) {
        // ... remember the absolute slot number where a request should be sent.
        uint64_t absolute_slot = owner->mac->getCurrentSlot() + init_offset + (tx_timeout - i) * tx_offset;
        slots.push_back(absolute_slot);
    }
    return slots;
}

bool LinkRenewalProcess::update() {
    if (remaining_attempts == 0)
        return false;
    if (!owner->mac->isThereMoreData(owner->getLinkId()))
        return false;
    // Check if the current slot is one during which a request should be sent.
    uint64_t current_slot = owner->mac->getCurrentSlot();
    auto it = std::find(absolute_request_slots.begin(), absolute_request_slots.end(), current_slot);
    // If it is, return that a request should now be sent.
    if (it != absolute_request_slots.end()) {
        remaining_attempts--;
        return true;
    }
    return false;
}