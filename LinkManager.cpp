//
// Created by Sebastian Lindner on 10.11.20.
//

#include <cassert>
#include "LinkManager.hpp"
#include "coutdebug.hpp"
#include "MCSOTDMA_Mac.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManager::LinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac)
	: link_id(link_id), reservation_manager(reservation_manager),
	link_establishment_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */,
	  mac(mac), traffic_estimate_queue_lengths(traffic_estimate_num_values) {}

const MacId& LinkManager::getLinkId() const {
	return this->link_id;
}

void LinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << "LinkManager::notifyOutgoing on link '" << link_id.getId() << "'";
	
	// Update the moving average traffic estimate.
	updateTrafficEstimate(num_bits);
	
	// Check establishment status.
	// If the link is established...
	if (link_establishment_status == Status::link_established) {
		coutd << ": link already established";
	// If the link is not yet established...
	} else {
		// ... and we've created the request and are just waiting for a reply ...
		if (link_establishment_status == Status::awaiting_reply) {
			coutd << ": link is being established and currently awaiting reply. Doing nothing." << std::endl;
			// ... then do nothing.
			return;
		// ... and link establishment has not yet been started ...
		} else if (link_establishment_status == Status::link_not_established) {
			coutd << ": link is not established. Starting link establishment" << std::endl;
			// Prepare a link request and inject it into the RLC sublayer above.
			L2Packet* request = prepareLinkEstablishmentRequest();
			mac->injectIntoUpper(request);
			// We are now awaiting a reply.
			this->link_establishment_status = Status::awaiting_reply;
		} else {
			throw std::runtime_error("Unsupported LinkManager::Status: '" + std::to_string(link_not_established) + "'.");
		}
	}
}

void LinkManager::notifyIncoming(unsigned long num_bits) {
		throw std::runtime_error("not implemented");
}

double LinkManager::getCurrentTrafficEstimate() const {
	if (traffic_estimate_index == 0)
		return 0.0; // No values were recorded yet.
	double moving_average = 0.0;
	for (auto it = traffic_estimate_queue_lengths.begin(); it < traffic_estimate_queue_lengths.end(); it++)
		moving_average += (*it);
	// Differentiate between a full window and a non-full window.
	return traffic_estimate_index < traffic_estimate_num_values ? moving_average / ((double) traffic_estimate_index) : moving_average / ((double) traffic_estimate_num_values);
}

L2Packet* LinkManager::prepareLinkEstablishmentRequest() {
	L2Packet* request = new L2Packet();
	// Query ARQ sublayer whether this link should be ARQ protected.
	bool link_should_be_arq_protected = mac->shouldLinkBeArqProtected(this->link_id);
	// Instantiate base header.
	auto* base_header = new L2HeaderBase(this->getLinkId(), 0, 0, 0, 0);
	request->addPayload(base_header, nullptr);
	// Instantiate request header.
	auto* request_header = new L2HeaderLinkEstablishmentRequest(link_id, link_should_be_arq_protected, 0, 0, 0);
	auto* body = new ProposalPayload(this->num_proposed_channels, this->num_proposed_slots);
	request->addPayload(request_header, body);
	return request;
}

void LinkManager::setProposalDimension(unsigned int num_candidate_channels, unsigned int num_candidate_slots) {
	this->num_proposed_channels = num_candidate_channels;
	this->num_proposed_slots = num_candidate_slots;
}

const unsigned int& LinkManager::getTrafficEstimateWindowSize() const {
	return this->traffic_estimate_num_values;
}

unsigned long LinkManager::estimateCurrentNumSlots() const {
	unsigned long traffic_estimate = (unsigned long) getCurrentTrafficEstimate(); // in bits.
	unsigned long datarate = mac->getCurrentDatarate(); // in bits/slot.
	return traffic_estimate / datarate; // in slots.
}

unsigned int LinkManager::getNumPendingReservations() const {
	return this->num_pending_reservations;
}

void LinkManager::updateTrafficEstimate(unsigned long num_bits) {
	// If the window hasn't been filled yet.
	if (traffic_estimate_index <= traffic_estimate_num_values - 1) {
		traffic_estimate_queue_lengths.at(traffic_estimate_index) = num_bits;
		traffic_estimate_index++;
	// If it has, kick out an old value.
	} else {
		for (size_t i = 1; i < traffic_estimate_queue_lengths.size(); i++) {
			traffic_estimate_queue_lengths.at(i - 1) = traffic_estimate_queue_lengths.at(i);
		}
		traffic_estimate_queue_lengths.at(traffic_estimate_queue_lengths.size() - 1) = num_bits;
	}
}

int32_t LinkManager::getEarliestReservationSlotOffset(int32_t start_slot, const Reservation& reservation) const {
	if (current_reservation_table == nullptr)
		throw std::runtime_error("LinkManager::getEarliestReservationSlotOffset has an unset reservation table.");
	return current_reservation_table->findEarliestOffset(start_slot, reservation);
}

void LinkManager::notifyOnOutgoingPacket(TUHH_INTAIRNET_MCSOTDMA::L2Packet* packet) {
	throw std::runtime_error("not implemeted");
}

void LinkManager::computeProposal(L2Packet* request) {
//	assert(request->getHeaders().size() == 2 && "There should be a base header and the request header.");
	assert(request->getPayloads().size() == 2 && "There should be a nullptr base payload and the request payload.");
//	auto* proposal_header = (L2HeaderLinkEstablishmentRequest*) request->getHeaders().at(1);
	auto* proposal_body = (ProposalPayload*) request->getPayloads().at(1);
	
	// Find resource proposals...
	// ... get the P2P reservation tables sorted by their numbers of idle slots ...
	auto table_priority_queue = reservation_manager->getSortedP2PReservationTables();
	// ... until we have considered the target number of channels ...
	unsigned long required_num_slots = estimateCurrentNumSlots();
	proposal_body->num_slots_per_candidate = this->num_proposed_slots;
	for (size_t num_channels_considered = 0; num_channels_considered < this->num_proposed_channels; num_channels_considered++) {
		if (table_priority_queue.empty()) // we could just stop here, but we're throwing an error to be aware when it happens
			throw std::runtime_error("LinkManager::prepareLinkEstablishmentRequest has considered " + std::to_string(num_channels_considered) + " out of " + std::to_string(num_proposed_channels) + " and there are no more.");
		// ... get the next reservation table ...
		ReservationTable* table = table_priority_queue.top();
		table_priority_queue.pop();
		// ... and try to find candidate slots.
		std::vector<int32_t> candidate_slots = table->findCandidateSlots(this->minimum_slot_offset_for_new_slot_reservations, this->num_proposed_slots, required_num_slots);
		
		// Fill proposal.
		proposal_body->proposed_channels.push_back(table->getLinkedChannel()); // Frequency channel.
		proposal_body->num_candidates.push_back(candidate_slots.size()); // Number of candidates (could be fewer than the target).
		for (int32_t slot : candidate_slots) // The candidate slots.
			proposal_body->proposed_slots.push_back(slot);
	}
}
