//
// Created by Sebastian Lindner on 01/12/22.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMAP_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMAP_HPP

#include <vector>
#include <sstream>
#include <algorithm>
#include "ReservationTable.hpp"
#include "Reservation.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

/** Container that saves the resources that were locked or scheduled during link establishment. */
class ReservationMap {
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
		return scheduled_resources.size() + locked_resources.size();
	}			
	
	void clear() {				
		this->scheduled_resources.clear();
		this->locked_resources.clear();
	}

	/** 	 
	 * @throws std::invalid_argument if any resource was not locked
	 * */
	void unlock_either_id(const MacId &id1, const MacId &id2) {				
		for (const auto& pair : locked_resources) {
			ReservationTable *table = pair.first;
			// skip SH reservations
			if (table->getLinkedChannel() != nullptr && table->getLinkedChannel()->isSH())
				continue;
			int slot_offset = pair.second - this->num_slots_since_creation;
			if (slot_offset > 0) {
				if (!table->getReservation(slot_offset).isLocked() && !table->getReservation(slot_offset).isIdle()) {							
					std::stringstream ss;
					ss << "ReservationMap::unlock cannot unlock reservation in " << slot_offset << " slots. Its status is: " << table->getReservation(slot_offset) << " when it should be locked.";
					throw std::invalid_argument(ss.str());
				} else 
					table->unlock_either_id(slot_offset, id1, id2);							
			}
		}		
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
	void unschedule(std::vector<Reservation::Action> expected_actions) {
		for (const auto& pair : scheduled_resources) {
			ReservationTable *table = pair.first;
			int slot_offset = pair.second - this->num_slots_since_creation;
			if (slot_offset > 0) {
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
				} else 
					table->mark(slot_offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
			}
		}
	}	

	protected:		
		std::vector<std::pair<ReservationTable*, int>> scheduled_resources;				
		std::vector<std::pair<ReservationTable*, int>> locked_resources;		
		/** Keep track of the number of time slots since creation, so that the slot offsets can be normalized to the current time. */
		int num_slots_since_creation;
};

}

#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMAP_HPP