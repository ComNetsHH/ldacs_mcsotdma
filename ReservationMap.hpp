//
// Created by Sebastian Lindner on 01/12/22.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMAP_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMAP_HPP

#include <vector>
#include <sstream>

namespace TUHH_INTAIRNET_MCSOTDMA {

/** Container that saves the resources that were locked or scheduled during link establishment. */
class ReservationMap {
public:
	ReservationMap() : num_slots_since_creation(0) {};		

	void merge(const ReservationMap& other) {
		for (const auto& pair : other.resource_reservations)
			resource_reservations.push_back(pair);				
	}

	void add(ReservationTable *table, int slot_offset) {
		resource_reservations.push_back({table, slot_offset});
	}

	void onSlotStart() {
		num_slots_since_creation++;
	}

	size_t size() const {
		return resource_reservations.size();
	}			
	bool anyLocks() const {
		return size() > 0;
	}
	void clear() {				
		this->resource_reservations.clear();
	}

	/** 
	 * Assuming that all here-saved resources were locked, it first checks that they are, and then unlocks them.
	 * @throws std::invalid_argument if any resource was not locked
	 * */
	void unlock(const MacId &id) {				
		for (const auto& pair : resource_reservations) {
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
					table->unlock(slot_offset, id);							
			}
		}
		// no need to manually unlock RX and TX tables
		// because the table->mark takes care of that auto-magically									
	}		

	/** 
	 * Assuming that all here-saved resources were scheduled, it first checks that they are, and then sets them to IDLE.
	 * @throws std::invalid_argument if any resource was not scheduled
	 * */
	void unschedule() {
		for (const auto& pair : resource_reservations) {
			ReservationTable *table = pair.first;
			unsigned int slot_offset = pair.second - this->num_slots_since_creation;
			if (slot_offset > 0) {
				if (!(table->getReservation(slot_offset).isTx() || table->getReservation(slot_offset).isRx())) {							
					std::stringstream ss;
					ss << "ReservationMap::unlock cannot unschedule reservation in " << slot_offset << " slots. Its status is: " << table->getReservation(slot_offset) << " when it should be TX or RX.";
					throw std::invalid_argument(ss.str());
				} else 
					table->mark(slot_offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
			}
		}
	}	

	protected:
		/** Local resources that were locked. */
		std::vector<std::pair<ReservationTable*, int>> resource_reservations;
		/** Keep track of the number of time slots since creation, so that the slot offsets can be normalized to the current time. */
		int num_slots_since_creation;
};

}

#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMAP_HPP