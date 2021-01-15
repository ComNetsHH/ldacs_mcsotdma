//
// Created by seba on 1/14/21.
//

#include <cassert>
#include "LinkRenewalProcess.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"

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

void LinkRenewalProcess::processLinkReply(const L2HeaderLinkEstablishmentReply *header,
                                          const LinkManager::ProposalPayload *payload) {
    // Make sure we're expecting a reply.
    if (owner->link_establishment_status != owner->Status::awaiting_reply)
        throw std::runtime_error("LinkManager for ID '" + std::to_string(owner->link_id.getId()) + "' received a link reply but its state is '" + std::to_string(owner->link_establishment_status) + "'.");
    // The link has now been established!
    // So update the status.
    owner->link_establishment_status = owner->Status::link_established;
    owner->mac->notifyAboutNewLink(owner->link_id);
    assert(payload->proposed_channels.size() == 1);
    owner->assign(payload->proposed_channels.at(0));
    // And mark the reservations.
    // We've received a reply, so we have initiated this link, so we are the transmitter.
    owner->tx_timeout = owner->default_tx_timeout;
    owner->markReservations(owner->tx_timeout, 0, owner->tx_offset, owner->tx_burst_num_slots, owner->link_id, Reservation::TX);
    // Refresh the link renewal process.
    configure(owner->link_renewal_attempts, owner->tx_timeout, 0, owner->tx_offset);
    coutd << "link is now established";
}