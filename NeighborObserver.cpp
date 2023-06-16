// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "NeighborObserver.hpp"
#include <iostream>

using namespace TUHH_INTAIRNET_MCSOTDMA;

NeighborObserver::NeighborObserver(unsigned int max_time_slots_until_neighbor_not_active_anymore) : max_last_seen_val(max_time_slots_until_neighbor_not_active_anymore), first_neighbor_avg_last_seen(MovingAverage(this->num_time_slots_to_average)) {}

void NeighborObserver::reportActivity(const MacId& id) {
	uint64_t num_time_slots_since_last_seen = updateLastSeenCounter(id);
	updateAvgLastSeen(id, num_time_slots_since_last_seen);
}

uint64_t NeighborObserver::updateLastSeenCounter(const MacId &id) {
	auto it = active_neighbors.find(id);
	uint64_t num_time_slots_since_last_seen;
	// if id does not exist
	if (it == active_neighbors.end()) {
		// add it
		num_time_slots_since_last_seen = 0;
		active_neighbors.insert({id, num_time_slots_since_last_seen}); 		
	// if id already exists
	} else {  
		// get the current value
		num_time_slots_since_last_seen = (*it).second;
		// and reset to zero
		(*it).second = 0;  // then set the counter value to zero slots	
	}
	return num_time_slots_since_last_seen;
}

void NeighborObserver::updateAvgLastSeen(const MacId &id, uint64_t num_time_slots_since_last_seen) {	
	try {
		if (first_neighbor_id == SYMBOLIC_ID_UNSET) 		
			first_neighbor_id = MacId(id);
		if (id == first_neighbor_id && num_time_slots_since_last_seen > 0)
			first_neighbor_avg_last_seen.put(num_time_slots_since_last_seen);
	} catch (const std::exception &e) {		
		throw std::runtime_error("error updating first-neighbor average time between beacons, with first neighbor id=" + std::to_string(id.getId()) + ", error is: " + std::string(e.what()));
	}	
	auto it = avg_last_seen.find(id);
	// if id does not exist
	if (it == avg_last_seen.end()) {
		// add it		
		auto pair = avg_last_seen.emplace(id, this->num_time_slots_to_average);		
		// add the value only if this is not a new observation
		if (num_time_slots_since_last_seen > 0 && pair.second)
			pair.first->second.put(num_time_slots_since_last_seen);
	// if id already exists
	} else {
		(*it).second.put(num_time_slots_since_last_seen);
	}
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
				try {
					advertised_broadcast_slots.erase(broadcast_slot_it);			
					erased_broadcast_element = true;
				} catch (const std::exception &e) {
					throw std::runtime_error("NeighborObserver error during erase (1st): " + std::string(e.what()));
				}								
			}
		}
		// and remove it if it hasn't been reported for too long
		if (it->second >= this->max_last_seen_val) {
			try {				
				it = active_neighbors.erase(it);				
			} catch (const std::exception &e) {
					throw std::runtime_error("NeighborObserver error during erase (2nd): " + std::string(e.what()));
				}
			if (!erased_broadcast_element && broadcast_slot_it != advertised_broadcast_slots.end()) {				
				try {
					advertised_broadcast_slots.erase(broadcast_slot_it);
				} catch (const std::exception &e) {
					throw std::runtime_error("NeighborObserver error during erase (3rd): " + std::string(e.what()));
				}				
			}
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

double NeighborObserver::getAvgBeaconDelay() const {
	double avg = 0.0;
	size_t i = 0;
	for (auto pair : avg_last_seen) {
		double current_avg = pair.second.get();
		if (current_avg > 0.0) {
			avg += current_avg;
			i++;
		}
	}
	return avg / ((double) i);
}

double NeighborObserver::getAvgFirstNeighborBeaconDelay() const {
	return first_neighbor_avg_last_seen.get();
}