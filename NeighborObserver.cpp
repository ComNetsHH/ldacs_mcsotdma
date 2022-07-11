/*
 * Created by Sebastian Lindner on Tue Oct 05 2021
 *
 * Copyright (c) 2021 Hamburg University of Technology
 */

#include "NeighborObserver.hpp"
#include <iostream>

using namespace TUHH_INTAIRNET_MCSOTDMA;

NeighborObserver::NeighborObserver(unsigned int max_time_slots_until_neighbor_not_active_anymore) : max_last_seen_val(max_time_slots_until_neighbor_not_active_anymore) {}

void NeighborObserver::reportActivity(const MacId& id) {
	auto it = active_neighbors.find(id);
	// if id does not exist
	if (it == active_neighbors.end())
		active_neighbors.insert({id, 0}); // add it
	else  // if it does
		(*it).second = 0;  // then set the last-seen value to zero slots	
}

void NeighborObserver::onSlotEnd() {
	// go through all active neighbors
	std::vector<std::map<MacId, unsigned int>::iterator> to_delete;
	for (auto it = active_neighbors.begin(); it != active_neighbors.end(); ) {
		// increment its last-seen value
		it->second++;
		// decrement its next advertised broadcast
		const MacId &id = it->first;
		auto broadcast_slot_it = advertised_broadcast_slots.find(id);
		bool erased_broadcast_element = false;
		if (broadcast_slot_it != advertised_broadcast_slots.end()) {									
			// decrement
			if (broadcast_slot_it->second > 0) 
				(*broadcast_slot_it).second--;				
			else { // or erase if it goes beyond zero
				advertised_broadcast_slots.erase(broadcast_slot_it);			
				erased_broadcast_element = true;
			}
		}
		// and remove it if it hasn't been reported for too long
		if (it->second >= this->max_last_seen_val) {
			it = active_neighbors.erase(it);
			if (!erased_broadcast_element)
				advertised_broadcast_slots.erase(broadcast_slot_it);
		} else
			++it;
	}	
}

size_t NeighborObserver::getNumActiveNeighbors() const {
	return active_neighbors.size();
}

bool NeighborObserver::isActive(const MacId& id) const {
	return active_neighbors.find(id) == active_neighbors.end();
}

std::vector<MacId> NeighborObserver::getActiveNeighbors() const {
	std::vector<MacId> ids;
	for (const auto& pair : active_neighbors)		
		ids.push_back(pair.first);
	return ids;
}

void NeighborObserver::reportBroadcastSlotAdvertisement(const MacId& id, unsigned int advertised_slot_offset) {
	auto it = advertised_broadcast_slots.find(id);
	// if id does not exist
	if (it == advertised_broadcast_slots.end())
		advertised_broadcast_slots.insert({id, advertised_slot_offset}); // add it
	else  // if it does
		(*it).second = advertised_slot_offset;  // then set the advertised value
}

unsigned int NeighborObserver::getNextExpectedBroadcastSlotOffset(const MacId &id) const {
	auto it = advertised_broadcast_slots.find(id);	
	if (it != advertised_broadcast_slots.end())
		return (*it).second;
	else  
		throw std::invalid_argument("no saved next broadcast slot for ID " + std::to_string(id.getId()));
}

void NeighborObserver::clearAdvertisedLinkProposals(const MacId &id) {
	auto it = advertised_link_proposals.find(id);
	if (it != advertised_link_proposals.end())
		it->second.clear();
}

void NeighborObserver::addAdvertisedLinkProposal(const MacId &id, unsigned long current_slot, const LinkProposal &proposal) {		
	auto it = advertised_link_proposals.find(id);
	if (it == advertised_link_proposals.end()) {		
		auto proposals = std::vector<std::pair<unsigned long, LinkProposal>>();
		proposals.push_back({current_slot, LinkProposal(proposal)});
		advertised_link_proposals.insert({id, proposals});
	} else 
		it->second.push_back({current_slot, LinkProposal(proposal)});	
}

std::vector<LinkProposal> NeighborObserver::getAdvertisedLinkProposals(const MacId &id, const unsigned long current_slot) const {
	std::vector<LinkProposal> valid_proposals;
	auto it = advertised_link_proposals.find(id);
	if (it != advertised_link_proposals.end()) {
		const std::vector<std::pair<unsigned long, LinkProposal>> &proposals = (*it).second;
		for (const auto &item : proposals) {
			const unsigned long &slot_when_saved = item.first;
			unsigned long num_elapsed_slots = current_slot - slot_when_saved;
			const LinkProposal &proposal = item.second;
			LinkProposal normalized_proposal = LinkProposal(proposal);
			normalized_proposal.slot_offset -= num_elapsed_slots;
			if (normalized_proposal.slot_offset > 0)
				valid_proposals.push_back(normalized_proposal);
		}
	}
	return valid_proposals;
}