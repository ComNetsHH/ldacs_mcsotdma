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
    absolute_request_slots = scheduleRequests(tx_timeout, init_offset, tx_offset);
}

std::vector<uint64_t> LinkRenewalProcess::scheduleRequests(unsigned int tx_timeout, unsigned int init_offset,
                                                       unsigned int tx_offset) const {
    std::vector<uint64_t> slots;
    // For each transmission burst from last to first according to this reservation...
    for (long i = 0, offset = init_offset + (tx_timeout-1)*tx_offset; slots.size() < num_renewal_attempts && offset >= init_offset; offset -= tx_offset, i++) {
        // ... add every second burst
        if (i % 2 == 1)
            slots.push_back(owner->mac->getCurrentSlot() + offset);
    }
    return slots;
}

bool LinkRenewalProcess::shouldSendRequest() {
    // Check if the current slot is one during which a request should be sent.
    bool should_send_request = false;
    for (auto it = absolute_request_slots.begin(); it != absolute_request_slots.end(); it++) {
        uint64_t current_slot = *it;
        if (current_slot == owner->mac->getCurrentSlot()) {
            absolute_request_slots.erase(it);
            it--; // Update iterator as the vector has shrunk.
            if (owner->mac->isThereMoreData(owner->getLinkId()))
                should_send_request = true;
        } else if (current_slot < owner->mac->getCurrentSlot())
            throw std::invalid_argument("LinkRenewalProcess::shouldSendRequest has missed a scheduled request.");
    }
    return should_send_request;
}

void LinkRenewalProcess::processLinkReply(const L2HeaderLinkEstablishmentReply*& header,
                                          const LinkManager::ProposalPayload*& payload) {
    // Make sure we're expecting a reply.
    if (owner->link_establishment_status != owner->Status::awaiting_reply)
        throw std::runtime_error("LinkManager for ID '" + std::to_string(owner->link_id.getId()) + "' received a link reply but its state is '" + std::to_string(owner->link_establishment_status) + "'.");
    // The link has now been established!
    // So shouldSendRequest the status.
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

void LinkRenewalProcess::onTransmissionSlot() {
    owner->tx_timeout--;
    if (owner->tx_timeout == owner->TIMEOUT_THRESHOLD_TRIGGER) {
        coutd << "Timeout threshold reached -> triggering new link request!" << std::endl;
        if (owner->link_establishment_status == owner->link_established) {
            owner->link_establishment_status = owner->link_about_to_expire;
            coutd << "set status to 'link_about_to_expire'." << std::endl;
        }
    }
}

void LinkRenewalProcess::processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header,
                                            const LinkManager::ProposalPayload*& payload, const MacId& origin) {
    auto viable_candidates = findViableCandidatesInRequest(
            (L2HeaderLinkEstablishmentRequest*&) header,
            (LinkManager::ProposalPayload*&) payload);
    if (!viable_candidates.empty()) {
        // Choose a candidate out of the set.
        auto chosen_candidate = viable_candidates.at(owner->getRandomInt(0, viable_candidates.size()));
        coutd << " -> picked candidate (" << chosen_candidate.first->getCenterFrequency() << "kHz, offset " << chosen_candidate.second << ") -> ";
        // Prepare a link reply.
        L2Packet* reply = owner->prepareLinkEstablishmentReply(origin);
        // Populate the payload.
        const FrequencyChannel* reply_channel = chosen_candidate.first;
        assert(reply->getPayloads().size() == 2);
        auto* reply_payload = (LinkManager::ProposalPayload*) reply->getPayloads().at(1);
        reply_payload->proposed_channels.push_back(reply_channel);
        int32_t slot_offset = chosen_candidate.second;
        // Pass it on to the corresponding LinkManager (this could've been received on the broadcast channel).
        coutd << "passing on to corresponding LinkManager -> ";
        unsigned int timeout = ((L2HeaderLinkEstablishmentRequest*&) header)->timeout,
                offset = ((L2HeaderLinkEstablishmentRequest*&) header)->offset,
                length = ((L2HeaderLinkEstablishmentRequest*&) header)->length_next;

        // The request may have been received by the broadcast link manager,
        // while the reply must be sent on a unicast channel,
        // so we have to forward the reply to the corresponding P2P LinkManager.
        owner->mac->forwardLinkReply(reply, reply_channel, slot_offset, timeout, offset, length);
    } else
        coutd << "no candidates viable. Doing nothing." << std::endl;
}

std::vector<std::pair<const FrequencyChannel *, unsigned int>>
LinkRenewalProcess::findViableCandidatesInRequest(L2HeaderLinkEstablishmentRequest *&header,
                                                  LinkManager::ProposalPayload *&payload) const {
        assert(payload && "LinkManager::findViableCandidatesInRequest for nullptr ProposalPayload*");
        const MacId& dest_id = header->icao_dest_id;
        if (payload->proposed_channels.empty())
            throw std::invalid_argument("LinkManager::findViableCandidatesInRequest for an empty proposal.");

        // Go through all proposed channels...
        std::vector<std::pair<const FrequencyChannel*, unsigned int>> viable_candidates;
        for (size_t i = 0; i < payload->proposed_channels.size(); i++) {
            const FrequencyChannel* channel = payload->proposed_channels.at(i);
            coutd << " -> proposed channel " << channel->getCenterFrequency() << "kHz:";
            // ... and all slots proposed on this channel ...
            unsigned int num_candidates_on_this_channel = payload->num_candidates.at(i);
            for (size_t j = 0; j < num_candidates_on_this_channel; j++) {
                unsigned int slot_offset = payload->proposed_slots.at(j);
                coutd << " @" << slot_offset;
                // ... and check if they're idle for us ...
                const ReservationTable* table = owner->reservation_manager->getReservationTable(channel);
                // ... if they are, then save them.
                if (table->isIdle(slot_offset, payload->num_slots_per_candidate) && owner->mac->isTransmitterIdle(slot_offset, payload->num_slots_per_candidate)) {
                    coutd << " (viable)";
                    viable_candidates.emplace_back(channel, slot_offset);
                } else
                    coutd << " (busy)";
            }
        }
        return viable_candidates;
}
