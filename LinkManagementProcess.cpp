//
// Created by seba on 1/14/21.
//

#include <cassert>
#include "LinkManagementProcess.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManagementProcess::LinkManagementProcess(LinkManager *owner) : owner(owner) {}

void LinkManagementProcess::configure(unsigned int num_renewal_attempts, unsigned int tx_timeout, unsigned int init_offset,
                                      unsigned int tx_offset) {
    this->num_renewal_attempts = num_renewal_attempts;
    // Schedule the absolute slots for sending requests.
    absolute_request_slots = scheduleRequests(tx_timeout, init_offset, tx_offset);
}

std::vector<uint64_t> LinkManagementProcess::scheduleRequests(unsigned int tx_timeout, unsigned int init_offset,
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

void LinkManagementProcess::processLinkReply(const L2HeaderLinkEstablishmentReply*& header,
                                             const LinkManager::ProposalPayload*& payload) {
    // Make sure we're expecting a reply.
    if (owner->link_establishment_status != owner->Status::awaiting_reply)
        throw std::runtime_error("LinkManager for ID '" + std::to_string(owner->link_id.getId()) + "' received a link reply but its state is '" + std::to_string(owner->link_establishment_status) + "'.");
    // The link has now been established!
    // So hasControlMessage the status.
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

void LinkManagementProcess::onTransmissionSlot() {
    owner->tx_timeout--;
    if (owner->tx_timeout == owner->TIMEOUT_THRESHOLD_TRIGGER) {
        coutd << "Timeout threshold reached -> triggering new link request!" << std::endl;
        if (owner->link_establishment_status == owner->link_established) {
            owner->link_establishment_status = owner->link_about_to_expire;
            coutd << "set status to 'link_about_to_expire'." << std::endl;
        }
    }
}

void LinkManagementProcess::processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header,
                                               const LinkManager::ProposalPayload*& payload, const MacId& origin) {
    auto viable_candidates = findViableCandidatesInRequest(
            (L2HeaderLinkEstablishmentRequest*&) header,
            (LinkManager::ProposalPayload*&) payload);
    if (!viable_candidates.empty()) {
        // Choose a candidate out of the set.
        auto chosen_candidate = viable_candidates.at(owner->getRandomInt(0, viable_candidates.size()));
        coutd << " -> picked candidate (" << chosen_candidate.first->getCenterFrequency() << "kHz, offset " << chosen_candidate.second << ") -> ";
        // Prepare a link reply.
        L2Packet* reply = prepareReply(origin);
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
LinkManagementProcess::findViableCandidatesInRequest(L2HeaderLinkEstablishmentRequest *&header,
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

L2Packet *LinkManagementProcess::prepareRequest() const {
    auto* request = new L2Packet();
    // Query ARQ sublayer whether this link should be ARQ protected.
    bool link_should_be_arq_protected = owner->mac->shouldLinkBeArqProtected(owner->link_id);
    // Instantiate base header.
    auto* base_header = new L2HeaderBase(owner->mac->getMacId(), 0, 0, 0);
    request->addPayload(base_header, nullptr);
    // Instantiate request header.
    // If the link is not yet established, the request must be sent on the broadcast channel.
    MacId dest_id = owner->link_establishment_status == owner->link_not_established ? SYMBOLIC_LINK_ID_BROADCAST : owner->link_id;
    auto* request_header = new L2HeaderLinkEstablishmentRequest(dest_id, link_should_be_arq_protected, 0, 0, 0);
    auto* body = new LinkManager::ProposalPayload(owner->num_proposed_channels, owner->num_proposed_slots);
    request->addPayload(request_header, body);
    request->addCallback(owner);
    coutd << "prepared request" << std::endl;
    return request;
}

L2Packet *LinkManagementProcess::prepareReply(const MacId& destination_id) const {
    auto* reply = new L2Packet();
    // Base header.
    auto* base_header = new L2HeaderBase(owner->mac->getMacId(), 0, 0, 0);
    reply->addPayload(base_header, nullptr);
    // Reply header.
    auto* reply_header = new L2HeaderLinkEstablishmentReply();
    reply_header->icao_dest_id = destination_id;
    // Reply payload will be populated by receiveFromLower.
    auto* reply_payload = new LinkManager::ProposalPayload(1, 1);
    reply->addPayload(reply_header, reply_payload);
    return reply;
}


void LinkManagementProcess::establishLink() const {
    coutd << "establishing new link... ";
    if (owner->link_establishment_status == owner->link_not_established) {
        // Prepare a link request and inject it into the RLC sublayer above.
        L2Packet* request = prepareRequest();
        coutd << "prepared link establishment request... ";
        owner->mac->injectIntoUpper(request);
        coutd << "injected into upper layer... ";
        // We are now awaiting a reply.
        owner->link_establishment_status = LinkManager::Status::awaiting_reply;
        coutd << "updated status to 'awaiting_reply'." << std::endl;
    } else
        throw std::runtime_error("LinkManager::establishLink for link status: " + std::to_string(owner->link_establishment_status));
}

L2Packet* LinkManagementProcess::getControlMessage() {
    L2Packet* control_message = nullptr;
    if (hasPendingReply()) {
        auto it = scheduled_link_replies.find(owner->mac->getCurrentSlot());
        control_message = (*it).second;
        scheduled_link_replies.erase(owner->mac->getCurrentSlot());
    } else if (hasPendingRequest()) {
        control_message = prepareRequest(); // Sets the callback, s.t. the actual proposal is computed then.
        // Delete scheduled slot.
        for (auto it = absolute_request_slots.begin(); it != absolute_request_slots.end(); it++) {
            uint64_t current_slot = *it;
            if (current_slot == owner->mac->getCurrentSlot()) {
                absolute_request_slots.erase(it);
                it--; // Update iterator as the vector has shrunk.
            }
        }
    }
    return control_message;
}

bool LinkManagementProcess::hasControlMessage() {
    return hasPendingRequest() || hasPendingReply();
}

bool LinkManagementProcess::hasPendingRequest() {
    for (unsigned long current_slot : absolute_request_slots) {
        if (current_slot == owner->mac->getCurrentSlot()) {
            if (owner->mac->isThereMoreData(owner->getLinkId()))
                return true;
        } else if (current_slot < owner->mac->getCurrentSlot())
            throw std::invalid_argument("LinkManagementProcess::hasControlMessage has missed a scheduled request.");
    }
    return false;
}

bool LinkManagementProcess::hasPendingReply() {
    return !scheduled_link_replies.empty() && scheduled_link_replies.find(owner->mac->getCurrentSlot()) != scheduled_link_replies.end();
}

void LinkManagementProcess::scheduleLinkReply(L2Packet *reply, int32_t slot_offset, unsigned int timeout,
                                              unsigned int offset, unsigned int length) {
    uint64_t absolute_slot = owner->mac->getCurrentSlot() + slot_offset;
    auto it = scheduled_link_replies.find(absolute_slot);
    if (it != scheduled_link_replies.end())
        throw std::runtime_error("LinkManager::scheduleLinkReply wanted to schedule a link reply, but there's already one scheduled at slot " + std::to_string(absolute_slot) + ".");
    else {
        // ... schedule it.
        if (owner->current_reservation_table->isUtilized(slot_offset))
            throw std::invalid_argument("LinkManager::scheduleLinkReply for an already reserved slot.");
        owner->current_reservation_table->mark(slot_offset, Reservation(reply->getDestination(), Reservation::Action::TX));
        scheduled_link_replies[absolute_slot] = reply;
        coutd << "-> scheduled reply in " << slot_offset << " slots." << std::endl;
        // ... and mark reservations: we're sending a reply, so we're the receiver.
        owner->markReservations(timeout, slot_offset, offset, length, reply->getDestination(), Reservation::Action::RX);
    }
}