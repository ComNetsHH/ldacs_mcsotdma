//
// Created by Sebastian Lindner on 10.11.20.
//

#include "LinkManager.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManager::LinkManager(const MacId& link_id, ReservationManager& reservation_manager, MCSOTDMA_Mac& mac)
	: link_id(link_id), reservation_manager(reservation_manager),
	link_establishment_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */,
	  mac(mac), traffic_estimate_queue_lengths(traffic_estimate_num_values) {}

const MacId& LinkManager::getLinkId() const {
	return this->link_id;
}

void LinkManager::notifyOutgoing() {
	coutd << "LinkManager::notifyOutgoing on link '" << link_id.getId() << "'";
	// If the link is established...
	if (link_establishment_status == Status::link_established) {
		coutd << " is already established";
	// If the link is not yet established...
	} else {
		// ... and we've created the request and are just waiting for a reply ...
		if (link_establishment_status == Status::awaiting_reply) {
			coutd << " is awaiting reply. Doing nothing." << std::endl;
			// ... then do nothing.
			return;
		// ... and link establishment has not yet been started ...
		} else if (link_establishment_status == Status::link_not_established) {
			coutd << " starting link establishment" << std::endl;
			
		} else {
			throw std::runtime_error("Unsupported LinkManager::Status: '" + std::to_string(link_not_established) + "'.");
		}
	}
}

void LinkManager::notifyIncoming(unsigned int num_bits) {
	// Update the moving average traffic estimate.
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
	L2Packet* request;
	// Query ARQ sublayer whether this link should be ARQ protected.
	bool link_should_be_arq_protected = mac.shouldLinkBeArqProtected(this->link_id);
	// Instantiate header.
	auto* header = new L2HeaderLinkEstablishmentRequest(link_id, link_should_be_arq_protected, 0, 0, 0);
	// Find resource proposals.
	auto sorted_reservation_tables = reservation_manager.getSortedReservationTables();
	
//	ProposalPayload* payload = new ProposalPayload();
	return request;
}


unsigned int LinkManager::getNumSlotsToReserve() const {
	return this->num_slots_to_reserve;
}

void LinkManager::setNumSlotsToReserver(const unsigned  int& num_slots) {
	this->num_slots_to_reserve = num_slots;
}

void LinkManager::setProposalDimension(unsigned int num_channels, unsigned int num_slots) {
	this->num_proposed_channels = num_channels;
	this->num_proposed_slots = num_slots;
}

unsigned int LinkManager::getProposalNumChannels() const {
	return this->num_proposed_channels;
}

unsigned int LinkManager::getProposalNumSlots() const {
	return this->num_proposed_slots;
}

const unsigned int& LinkManager::getTrafficEstimateWindowSize() const {
	return this->traffic_estimate_num_values;
}

unsigned int LinkManager::ProposalPayload::getBits() const {
	// Should calculate bits from number of channels * number of slots.
	// But how many bits per (f, t)-pair?
	throw std::runtime_error("not implemented");
}
