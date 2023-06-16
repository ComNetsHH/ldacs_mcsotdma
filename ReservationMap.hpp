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

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMAP_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMAP_HPP

#include <vector>
#include <sstream>
#include <algorithm>
#include <limits>
#include "ReservationTable.hpp"
#include "Reservation.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

/** Container that saves the resources that were locked or scheduled during link establishment. */
class ReservationMap {

	friend class PPLinkManagerTests;
	friend class PPLinkManager;	

public:
	ReservationMap() : num_slots_since_creation(0) {};		

	void merge(const ReservationMap& other) {
		for (const auto& pair : other.scheduled_resources)
			scheduled_resources.push_back(pair);	
		for (const auto& pair : other.locked_resources)
			locked_resources.push_back(pair);	
	}

	void add_scheduled_resource(ReservationTable *table, int slot_offset) {
		scheduled_resources.push_back({table, slot_offset});
	}

	void add_locked_resource(ReservationTable *table, int slot_offset) {
		locked_resources.push_back({table, slot_offset});
	}

	void onSlotStart() {
		num_slots_since_creation++;
	}

	size_t size() const {
		return size_scheduled() + size_locked();
	}			
	size_t size_scheduled() const {
		return scheduled_resources.size();
	}
	size_t size_locked() const {
		return locked_resources.size();
	}
	
	void reset() {				
		this->scheduled_resources.clear();
		this->locked_resources.clear();
		this->num_slots_since_creation = 0;
	}	

	/** 	 
	 * @throws std::runtime_error if any resource was not locked
	 * */
	size_t unlock_either_id(const MacId &id1, const MacId &id2) {				
		size_t num_unlocked = 0;		
		for (const auto& pair : locked_resources) {
			ReservationTable *table = pair.first;
			// skip SH reservations
			if (table->getLinkedChannel() != nullptr && table->getLinkedChannel()->isSH())
				continue;			
			int slot_offset = pair.second - this->num_slots_since_creation;						
			if (slot_offset > 0) {				
				try {
					table->unlock_either_id(slot_offset, id1, id2);							
					num_unlocked++;					
				} catch (const id_mismatch &e) {					
					// do nothing
				} catch (const std::invalid_argument &e) {					
					// do nothing
				} catch (const std::exception &e) {
					throw std::runtime_error("ReservationMap::unlock_either_id error: " + std::string(e.what()));
				}
			}
		}		
		return num_unlocked;
	}		

	/** 	 
	 * @throws std::invalid_argument if any resource was not locked
	 * */
	void unlock(const MacId &id) {				
		unlock_either_id(id, id);
	}		

	/** 	 
	 * @throws std::invalid_argument if any resource was not scheduled
	 * */
	size_t unschedule(std::vector<Reservation::Action> expected_actions) {
		size_t num_unscheduled = 0;
		for (const auto& pair : scheduled_resources) {
			ReservationTable *table = pair.first;
			int slot_offset = pair.second - this->num_slots_since_creation;
			if (slot_offset >= 0) {
				auto action = table->getReservation(slot_offset).getAction();
				if (!std::any_of(expected_actions.begin(), expected_actions.end(), [action](const Reservation::Action &expected_action) {return expected_action == action;})) {							
					std::stringstream ss;
					ss << "ReservationMap::unlock cannot unschedule reservation in " << slot_offset << " slots";
					if (table->getLinkedChannel() != nullptr)
						ss << " on f=" << *table->getLinkedChannel();	
					ss << ". Its status is: " << table->getReservation(slot_offset) << " when it should be any from: ";
					for (const auto &a : expected_actions)
						ss << a << " "; 
					throw std::invalid_argument(ss.str());
				} else {					
					table->mark(slot_offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));					
					num_unscheduled++;
				}
			}
		}
		return num_unscheduled;
	}	

	std::pair<ReservationTable*, int> getNextTxReservation() const {
		int closest_time_slot = std::numeric_limits<int>::max();
		std::pair<ReservationTable*, int> best_match = {nullptr, 0};
		for (auto pair : scheduled_resources) {			
			int time_slot = pair.second - this->num_slots_since_creation;
			bool is_tx = pair.first->getReservation(pair.second - this->num_slots_since_creation).isTx();
			if (time_slot >= 0 && time_slot < closest_time_slot && is_tx) {
				closest_time_slot = time_slot;
				best_match = pair;
				best_match.second = time_slot;								
			}
		}		
		return best_match;
	}

	std::pair<ReservationTable*, int> getNextRxReservation() const {
		int closest_time_slot = std::numeric_limits<int>::max();
		std::pair<ReservationTable*, int> best_match = {nullptr, 0};
		for (auto pair : scheduled_resources) {			
			int time_slot = pair.second - this->num_slots_since_creation;
			bool is_rx = pair.first->getReservation(pair.second - this->num_slots_since_creation).isRx();
			if (time_slot >= 0 && time_slot < closest_time_slot && is_rx) {
				closest_time_slot = time_slot;
				best_match = pair;
				best_match.second = time_slot;
			}
		}
		return best_match;
	}

	protected:		
		std::vector<std::pair<ReservationTable*, int>> scheduled_resources;				
		std::vector<std::pair<ReservationTable*, int>> locked_resources;		
		/** Keep track of the number of time slots since creation, so that the slot offsets can be normalized to the current time. */
		int num_slots_since_creation;
};

}

#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMAP_HPP