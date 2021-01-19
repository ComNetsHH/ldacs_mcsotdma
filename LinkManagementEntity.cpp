//
// Created by seba on 1/14/21.
//

#include <cassert>
#include "LinkManagementEntity.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManagementEntity::LinkManagementEntity(LinkManager *owner) : owner(owner) {}

void LinkManagementEntity::configure(unsigned int num_renewal_attempts, unsigned int tx_timeout, unsigned int init_offset,
                                     unsigned int tx_offset) {
    this->num_renewal_attempts = num_renewal_attempts;
    // Schedule the absolute slots for sending requests.
    scheduled_requests = scheduleRequests(tx_timeout, init_offset, tx_offset);
}

std::vector<uint64_t> LinkManagementEntity::scheduleRequests(unsigned int tx_timeout, unsigned int init_offset,
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

void LinkManagementEntity::processLinkReply(const L2HeaderLinkEstablishmentReply*& header,
                                            const ProposalPayload*& payload) {
    // Make sure we're expecting a reply.
    if (owner->link_establishment_status != owner->Status::awaiting_reply)
        throw std::runtime_error("LinkManager for ID '" + std::to_string(owner->link_id.getId()) + "' received a link reply but its state is '" + std::to_string(owner->link_establishment_status) + "'.");
    assert(payload->proposed_channels.size() == 1);
    const FrequencyChannel* channel = payload->proposed_channels.at(0);

    // Configuring an initial channel...
    if (owner->current_channel == nullptr) {
        coutd << "assigning channel -> ";
        owner->assign(channel);
        tx_timeout = default_tx_timeout;
        coutd << "resetting timeout to " << tx_timeout << " -> marking TX reservations:";
        owner->markReservations(tx_timeout, 0, tx_offset, tx_burst_num_slots, owner->link_id, Reservation::TX);
        coutd << " -> configuring request slots -> ";
        configure(link_renewal_attempts, tx_timeout, 0, tx_offset);
        owner->link_establishment_status = owner->Status::link_established;
        owner->mac->notifyAboutNewLink(owner->link_id);
        coutd << "link is now established";
    // Renewing an existing link...
    } else {
        coutd << "renewing link -> ";
        if (channel == owner->current_channel) {
            coutd << "no channel change -> increasing timeout: " << tx_timeout;
            tx_timeout += default_tx_timeout;
            coutd << tx_timeout << " and marking TX reservations: ";
            owner->markReservations(tx_timeout, 0, tx_offset, tx_burst_num_slots, owner->link_id, Reservation::TX);
            coutd << " -> configuring request slots -> ";
            configure(link_renewal_attempts, tx_timeout, 0, tx_offset);
            coutd << "link status update: " << owner->link_establishment_status;
            owner->link_establishment_status = owner->Status::link_established;
            coutd << "->" << owner->link_establishment_status;
        } else {
            coutd << "channel change -> saving new channel -> ";
            next_channel = channel;
            coutd << "link status update: " << owner->link_establishment_status;
            owner->link_establishment_status = owner->Status::link_renewal_complete;
            coutd << "->" << owner->link_establishment_status << " -> ";
        }
    }
}

void LinkManagementEntity::onTransmissionBurst() {
    tx_timeout--;
    if (tx_timeout == 0) {
        coutd << "timeout reached -> setting this link to unestablished!" << std::endl;
        owner->link_establishment_status = LinkManager::link_not_established;
    }
}

void LinkManagementEntity::processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header,
                                              const ProposalPayload*& payload, const MacId& origin) {
    auto viable_candidates = findViableCandidatesInRequest(
            (L2HeaderLinkEstablishmentRequest*&) header,
            (ProposalPayload*&) payload);
    if (!viable_candidates.empty()) {
        // Choose a candidate out of the set.
        auto chosen_candidate = viable_candidates.at(owner->getRandomInt(0, viable_candidates.size()));
        coutd << " -> picked candidate (" << chosen_candidate.first->getCenterFrequency() << "kHz, offset " << chosen_candidate.second << ") -> ";
        // Prepare a link reply.
        L2Packet* reply = prepareReply(origin);
        // Populate the payload.
        const FrequencyChannel* reply_channel = chosen_candidate.first;
        assert(reply->getPayloads().size() == 2);
        auto* reply_payload = (ProposalPayload*) reply->getPayloads().at(1);
        reply_payload->proposed_channels.push_back(reply_channel);
        int32_t slot_offset = chosen_candidate.second;
        // Pass it on to the corresponding LinkManager (this could've been received on the broadcast channel).
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
LinkManagementEntity::findViableCandidatesInRequest(L2HeaderLinkEstablishmentRequest *&header,
                                                    ProposalPayload *&payload) const {
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

L2Packet *LinkManagementEntity::prepareRequest() const {
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
    auto* body = new ProposalPayload(num_proposed_channels, num_proposed_slots);
    request->addPayload(request_header, body);
    request->addCallback(owner);
    return request;
}

L2Packet *LinkManagementEntity::prepareReply(const MacId& destination_id) const {
    auto* reply = new L2Packet();
    // Base header.
    auto* base_header = new L2HeaderBase(owner->mac->getMacId(), 0, 0, 0);
    reply->addPayload(base_header, nullptr);
    // Reply header.
    auto* reply_header = new L2HeaderLinkEstablishmentReply();
    reply_header->icao_dest_id = destination_id;
    // Reply payload will be populated by receiveFromLower.
    auto* reply_payload = new ProposalPayload(1, 1);
    reply->addPayload(reply_header, reply_payload);
    return reply;
}


void LinkManagementEntity::establishLink() const {
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

L2Packet* LinkManagementEntity::getControlMessage() {
    L2Packet* control_message = nullptr;
    if (hasPendingReply()) {
        auto it = scheduled_replies.find(owner->mac->getCurrentSlot());
        control_message = (*it).second;
        // Delete scheduled entry.
        scheduled_replies.erase(owner->mac->getCurrentSlot());
    } else if (hasPendingRequest()) {
        control_message = prepareRequest(); // Sets the callback, s.t. the actual proposal is computed then.
        // Delete scheduled entry.
        for (auto it = scheduled_requests.begin(); it != scheduled_requests.end(); it++) { // it's a std::vector, so there's no find() and there may be multiples
            uint64_t current_slot = *it;
            if (current_slot == owner->mac->getCurrentSlot()) {
                scheduled_requests.erase(it);
                it--; // Update iterator as the vector has shrunk.
            }
        }
    }
    return control_message;
}

bool LinkManagementEntity::hasControlMessage() {
    return hasPendingRequest() || hasPendingReply();
}

bool LinkManagementEntity::hasPendingRequest() {
    for (unsigned long current_slot : scheduled_requests) {
        if (current_slot == owner->mac->getCurrentSlot()) {
            if (owner->mac->isThereMoreData(owner->getLinkId()))
                return true;
        } else if (current_slot < owner->mac->getCurrentSlot())
            throw std::invalid_argument("LinkManagementEntity::hasControlMessage has missed a scheduled request.");
    }
    return false;
}

bool LinkManagementEntity::hasPendingReply() {
    return !scheduled_replies.empty() && scheduled_replies.find(owner->mac->getCurrentSlot()) != scheduled_replies.end();
}

void LinkManagementEntity::scheduleLinkReply(L2Packet *reply, int32_t slot_offset, unsigned int timeout,
                                             unsigned int offset, unsigned int length) {
    uint64_t absolute_slot = owner->mac->getCurrentSlot() + slot_offset;
    auto it = scheduled_replies.find(absolute_slot);
    if (it != scheduled_replies.end())
        throw std::runtime_error("LinkManager::scheduleLinkReply wanted to schedule a link reply, but there's already one scheduled at slot " + std::to_string(absolute_slot) + ".");
    else {
        // ... schedule it.
        if (owner->current_reservation_table->isUtilized(slot_offset))
            throw std::invalid_argument("LinkManager::scheduleLinkReply for an already reserved slot.");
        owner->current_reservation_table->mark(slot_offset, Reservation(reply->getDestination(), Reservation::Action::TX));
        scheduled_replies[absolute_slot] = reply;
        coutd << "-> scheduled reply in " << slot_offset << " slots ";
        if (owner->link_establishment_status == LinkManager::link_not_established) {
            // ... and mark reservations: we're sending a reply, so we're the receiver.
             coutd << "-> first-time link setup; mark RX slots -> ";
            owner->markReservations(timeout, slot_offset, offset, length, reply->getDestination(), Reservation::Action::RX);
        } else {
            // ... renewal negotiation: might have to change the FrequencyChannel, so do nothing now.
        }
    }
}

void LinkManagementEntity::setTxTimeout(unsigned int value) {
    tx_timeout = value;
}

void LinkManagementEntity::setTxOffset(unsigned int value) {
    tx_offset = value;
}

unsigned int LinkManagementEntity::getTxTimeout() const {
    return tx_timeout;
}

unsigned int LinkManagementEntity::getTxOffset() const {
    return tx_offset;
}

unsigned int LinkManagementEntity::getMinOffset() const {
    return minimum_slot_offset_for_new_slot_reservations;
}

LinkManagementEntity::ProposalPayload *LinkManagementEntity::p2pSlotSelection() {
    auto* proposal = new ProposalPayload(num_proposed_channels, num_proposed_slots);

    // Find resource proposals...
    // ... get the P2P reservation tables sorted by their numbers of idle slots ...
    auto table_priority_queue = owner->reservation_manager->getSortedP2PReservationTables();
    // ... until we have considered the target number of channels ...
    tx_burst_num_slots = owner->estimateCurrentNumSlots();
    coutd << "p2pSlotSelection to reserve " << tx_burst_num_slots << " slots -> ";
    for (size_t num_channels_considered = 0; num_channels_considered < this->num_proposed_channels; num_channels_considered++) {
        if (table_priority_queue.empty()) // we could just stop here, but we're throwing an error to be aware when it happens
            throw std::runtime_error("LinkManager::prepareRequest has considered " + std::to_string(num_channels_considered) + " out of " + std::to_string(num_proposed_channels) + " and there are no more.");
        // ... get the next reservation table ...
        ReservationTable* table = table_priority_queue.top();
        table_priority_queue.pop();
        // ... and try to find candidate slots ...
        std::vector<int32_t> candidate_slots = table->findCandidateSlots(this->minimum_slot_offset_for_new_slot_reservations, this->num_proposed_slots, tx_burst_num_slots, true);
        // ... and lock them s.t. future proposals don't consider them.
        table->lock(candidate_slots);

        // Fill proposal.
        proposal->proposed_channels.push_back(table->getLinkedChannel()); // Frequency channel.
        proposal->num_candidates.push_back(candidate_slots.size()); // Number of candidates (could be fewer than the target).
        proposal->num_slots_per_candidate = tx_burst_num_slots;
        for (int32_t slot : candidate_slots) // The candidate slots.
            proposal->proposed_slots.push_back(slot);
    }
    return proposal;
}

unsigned int LinkManagementEntity::getTxBurstSlots() const {
    return tx_burst_num_slots;
}

void LinkManagementEntity::populateRequest(L2Packet*& request) {
    for (size_t i = 0; i < request->getHeaders().size(); i++) {
        L2Header* header = request->getHeaders().at(i);
        if (header->frame_type == L2Header::link_establishment_request) {
            // Set the destination ID (may be broadcast until now).
            auto* request_header = (L2HeaderLinkEstablishmentRequest*) header;
            request_header->icao_dest_id = owner->link_id;
            request_header->offset = tx_offset;
            request_header->timeout = default_tx_timeout;
            // Remember this request's number of slots.
            tx_burst_num_slots = owner->estimateCurrentNumSlots();
            request_header->length_next = tx_burst_num_slots;
            // Compute a current proposal.
            request->getPayloads().at(i) = p2pSlotSelection();
            coutd << "populated link request: " << *request_header << " -> ";
            break;
        }
    }
}

void LinkManagementEntity::onRequestTransmission() {
    // Upon a renewal request...
    if (owner->link_establishment_status != LinkManager::link_not_established) {
        // ... mark the next transmission burst as RX to receive the reply.
        owner->current_reservation_table->mark(tx_offset, Reservation(owner->getLinkId(), Reservation::Action::RX));
        // Upon initial requests...
    } else {
        // ... do nothing.
    }
}