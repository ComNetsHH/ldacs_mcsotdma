//
// Created by Sebastian Lindner on 06.10.20.
//

#include <stdexcept>
#include <algorithm>
#include <math.h>
#include <limits>
#include "ReservationTable.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ReservationTable::ReservationTable(uint32_t planning_horizon)
		: planning_horizon(planning_horizon), slot_utilization_vec(std::vector<Reservation>(uint64_t(planning_horizon * 2 + 1))), last_updated(), num_idle_future_slots(planning_horizon + 1), default_reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE) {
	// The planning horizon denotes how many slots we want to be able to look into future and past.
	// Since the current moment in time must also be represented, we need planning_horizon*2+1 values.
	// If we use UINT32_MAX, then we wouldn't be able to store 2*UINT32_MAX+1 in UINT64, so throw an exception if this is attempted.
	if (planning_horizon == UINT32_MAX)
		throw std::invalid_argument("Cannot instantiate a reservation table with a planning horizon of UINT32_MAX. It must be at least one slot less.");
	// In practice, allocating even much less results in a std::bad_alloc anyway...
}

ReservationTable::ReservationTable() : ReservationTable(512) {}

ReservationTable::ReservationTable(uint32_t planning_horizon, const Reservation& default_reservation) : ReservationTable(planning_horizon) {
	this->default_reservation = default_reservation;
	for (auto& it : slot_utilization_vec)
		it = default_reservation;
}

uint32_t ReservationTable::getPlanningHorizon() const {
	return this->planning_horizon;
}

Reservation* ReservationTable::mark(int32_t slot_offset, const Reservation& reservation) {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::mark planning_horizon=" + std::to_string(planning_horizon) + " smaller than queried slot_offset=" + std::to_string(slot_offset) + "!");
	// If the exact same reservation already exists, just return it and be done with it.
	if (getReservation(slot_offset) == reservation)
		return &this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
	// Ensure that linked hardware tables have capacity.
	if ((reservation.isAnyTx()) && transmitter_reservation_table != nullptr)
		if (!(transmitter_reservation_table->isIdle(slot_offset) || transmitter_reservation_table->isLocked(slot_offset)))
			throw no_tx_available_error("ReservationTable::mark(" + std::to_string(slot_offset) + ") can't forward TX reservation because the linked transmitter table is not idle.");
	if ((reservation.isAnyRx()) && !receiver_reservation_tables.empty()) {
		if (!std::any_of(receiver_reservation_tables.begin(), receiver_reservation_tables.end(), [slot_offset](ReservationTable* table) {
			return table->isIdle(slot_offset) || table->isLocked(slot_offset);
		})) {
			for (const auto& rx_table : receiver_reservation_tables)
				coutd << std::endl << "Problematic reservation: " << rx_table->getReservation(slot_offset) << std::endl;
			throw no_rx_available_error("ReservationTable::mark(" + std::to_string(slot_offset) + ") can't forward RX reservation because none out of " + std::to_string(receiver_reservation_tables.size()) + " linked receiver tables are idle.");
		}
	}
	bool currently_idle = this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).isIdle();
	this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)) = reservation;
	// Update the number of idle slots.
	if (currently_idle && !reservation.isIdle()) // idle -> non-idle
		num_idle_future_slots--;
	else if (!currently_idle && reservation.isIdle()) // non-idle -> idle
		num_idle_future_slots++;
	// If a transmitter table is linked, mark it there, too.
	if (reservation.isAnyTx() && transmitter_reservation_table != nullptr) {
		// Need a copy here s.t. the linked table's recursive call won't set all slots now.
		Reservation cpy = Reservation(reservation);
		cpy.setNumRemainingSlots(0);
		transmitter_reservation_table->mark(slot_offset, cpy);
	}
	// Same for receiver tables
	if (reservation.isAnyRx() && !receiver_reservation_tables.empty())
		for (ReservationTable* rx_table : receiver_reservation_tables) {
			if (rx_table->getReservation(slot_offset).isIdle()) {
				// Need a copy here s.t. the linked table's recursive call won't set all slots now.
				Reservation cpy = Reservation(reservation);
				cpy.setNumRemainingSlots(0);
				rx_table->mark(slot_offset, cpy);
				break;
			}
		}
	// If this is a multi-slot transmission reservation, set the following ones, too.
	if (reservation.getNumRemainingSlots() > 0) {
		Reservation::Action action = reservation.getAction();
		if (action == Reservation::TX)
			action = Reservation::TX_CONT;
		else if (action == Reservation::RX)
			action = Reservation::RX_CONT;
		Reservation next_reservation = Reservation(reservation.getTarget(), action, reservation.getNumRemainingSlots() - 1);
		mark(slot_offset + 1, next_reservation);
	}
	return &this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
}

bool ReservationTable::isUtilized(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::isUtilized for planning horizon smaller than queried offset!");
	return !this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).isIdle();
}

bool ReservationTable::isLocked(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::isLocked for planning horizon smaller than queried offset!");
	return this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).isLocked();
}

bool ReservationTable::anyTxReservations(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::anyTxReservations for planning horizon smaller than queried offset!");
	const Reservation& res = this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
	return res.isAnyTx();
}

bool ReservationTable::anyTxReservations(int32_t start, uint32_t length) const {
	if (length == 1)
		return anyTxReservations(start);
	if (!this->isValid(start, length))
		throw std::invalid_argument("ReservationTable::anyTxReservations invalid slot range: start=" + std::to_string(start) + " length=" + std::to_string(length));
	// A slot range contains a TX or TX_CONT if any slot does
	for (int32_t slot = start; slot < start + int32_t(length); slot++)
		if (anyTxReservations(slot))
			return true; // so a single busy one fails the check
	return false;
}

bool ReservationTable::anyRxReservations(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::anyRxReservations for planning horizon smaller than queried offset!");
	const Reservation& res = this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
	return res.isAnyRx();
}

bool ReservationTable::anyRxReservations(int32_t start, uint32_t length) const {
	if (length == 1)
		return anyRxReservations(start);
	if (!this->isValid(start, length))
		throw std::invalid_argument("ReservationTable::anyRxReservations invalid slot range: start=" + std::to_string(start) + " length=" + std::to_string(length));
	// A slot range contains a RX if any slot does
	for (int32_t slot = start; slot < start + int32_t(length); slot++)
		if (anyRxReservations(slot))
			return true; // so a single busy one fails the check
	return false;
}

bool ReservationTable::isIdle(int32_t slot_offset) const {
	return !this->isUtilized(slot_offset);
}

bool ReservationTable::isIdle(int32_t start, uint32_t length) const {
	if (length == 1)
		return this->isIdle(start);
	if (!this->isValid(start, length))
		throw std::invalid_argument("ReservationTable::isIdle invalid slot range: start=" + std::to_string(start) + " length=" + std::to_string(length));
	// A slot range is idle if ALL slots within are idle.
	for (int32_t slot = start; slot < start + int32_t(length); slot++)
		if (isUtilized(slot))
			return false; // so a single busy one fails the check
	return true;
}

bool ReservationTable::isUtilized(int32_t start, uint32_t length) const {
	// A slot range is utilized if any slot within is utilized.
	return !this->isIdle(start, length);
}

unsigned int ReservationTable::findEarliestIdleSlots(unsigned int start_offset, unsigned int burst_length, unsigned int burst_length_tx, bool rx_idle_during_first_slot) const {
	if (!isValid(start_offset, burst_length))
		throw std::invalid_argument("Invalid slot range!");
	if (transmitter_reservation_table == nullptr)
		throw std::runtime_error("ReservationTable::findEarliestIdleSlots for unset transmitter table.");

	for (unsigned int t = start_offset; t < planning_horizon; t++) {
		// Check if local table is idle...
		if (isIdle(t, burst_length)) {
			// ... check if the transmitter is idle for the first burst_length_tx slots...
			bool transmitter_idle = transmitter_reservation_table->isIdle(t, burst_length_tx);
			// ... check if a receiver is idle for the trailing burst_length_rx slots...
			unsigned int burst_length_rx = burst_length - burst_length_tx;
			bool receiver_idle;
			if (burst_length_rx == 0)
				receiver_idle = true;
			else {
				unsigned int t_rx = t + burst_length_tx;
				if (receiver_reservation_tables.empty())
					receiver_idle = true;
				else
					receiver_idle = std::any_of(receiver_reservation_tables.begin(), receiver_reservation_tables.end(), [t_rx, burst_length_rx](ReservationTable* table) {
						return table->isIdle(t_rx, burst_length_rx);
					});
			}
			// ... if a receiver must also be available during the first slot...
			if (rx_idle_during_first_slot) {
				if (receiver_reservation_tables.empty())
					receiver_idle = true;
				else
					receiver_idle = receiver_idle && std::any_of(receiver_reservation_tables.begin(), receiver_reservation_tables.end(), [t](ReservationTable* table) {
						return table->isIdle(t);
					});
			}

			if (transmitter_idle && receiver_idle)
				return t;
		}
	}
	throw std::range_error("No idle slot range could be found.");
}

//int32_t ReservationTable::findEarliestIdleRange(int32_t start, uint32_t length, bool consider_transmitter) const {
//	if (consider_transmitter && !transmitter_reservation_table)
//		throw std::runtime_error("ReservationTable::findEarliestIdleRange that should consider the transmitter with an unset transmitter_reservation_table.");
//	if (!isValid(start, length))
//		throw std::invalid_argument("Invalid slot range!");
//	for (int32_t i = start; i < int32_t(this->planning_horizon); i++) {
//		// Is this table idle?
//		if (this->isIdle(i, length)) {
//			if (!consider_transmitter) // Don't care about an idle transmitter?
//				return i; // Just return the slot since this table is idle.
//			else if (!transmitter_reservation_table->anyTxReservations(i, length)) // Do care? Then check whether it is idle, too.
//				return i;
//		}
//	}
//	throw std::runtime_error("ReservationTable::findEarliestIdleRange found no idle slot range of specified length.");
//}

bool ReservationTable::isValid(int32_t slot_offset) const {
	return std::abs(slot_offset) <= this->planning_horizon; // can't move more than one horizon into either direction of time.
}

bool ReservationTable::isValid(int32_t start, uint32_t length) const {
	if (length == 1)
		return this->isValid(start);
	return isValid(start) && isValid(start + length - 1);
}

const Timestamp& ReservationTable::getCurrentSlot() const {
	return this->last_updated;
}

void ReservationTable::update(uint64_t num_slots) {
	// Count the number of busy slots that go out of scope on the time domain.
	uint64_t num_busy_slots = 0;
	// Start counting at offset zero (current time slot) as history doesn't matter.
	for (size_t t = 0; t < num_slots; t++)
		if (!getReservation(t).isIdle())
			num_busy_slots++;
	num_idle_future_slots += num_busy_slots; // As these go out of scope, we may have more idle slots now.

	// Shift all elements to the front, old ones are overwritten.
	std::move(this->slot_utilization_vec.begin() + num_slots, this->slot_utilization_vec.end(), this->slot_utilization_vec.begin());
	// All new elements are initialized as idle.
	for (auto it = slot_utilization_vec.end() - 1; it >= slot_utilization_vec.end() - num_slots; it--)
		*it = default_reservation;
	last_updated += num_slots;
}

const std::vector<Reservation>& ReservationTable::getVec() const {
	return this->slot_utilization_vec;
}

uint64_t ReservationTable::convertOffsetToIndex(int32_t slot_offset) const {
	// The vector has planning_horizon-many past slots, one current slot, and planning_horizon-many future slots.
	// So planning_horizon+0 indicates the current slot, which is the basis for this relative access.
	return planning_horizon + slot_offset;
}

void ReservationTable::setLastUpdated(const Timestamp& timestamp) {
	last_updated = timestamp;
}

uint64_t ReservationTable::getNumIdleSlots() const {
	return this->num_idle_future_slots;
}

std::vector<unsigned int> ReservationTable::findCandidates(unsigned int num_slots, unsigned int min_offset, unsigned int burst_length, unsigned int burst_length_tx, bool rx_idle_during_first_slot) const {
	std::vector<unsigned int> start_slots;
	unsigned int last_offset = min_offset;
	for (size_t i = 0; i < num_slots; i++) {
		// Try to find another slot range.
		try {
			int32_t start_slot = findEarliestIdleSlots(last_offset, burst_length, burst_length_tx, rx_idle_during_first_slot);
			start_slots.push_back(start_slot);
			last_offset = start_slot + 1; // Next attempt, look later than current one.
		} catch (const std::range_error& e) {
			// This is thrown if no idle range can be found.
			break; // Stop if no more ranges can be found.
		} catch (const std::invalid_argument& e) {
			// This is thrown if the input is invalid (i.e. we are exceeding the planning horizon).
			break; // Stop if no more ranges can be found.
		} // all other exceptions should still end execution
	}
	return start_slots;
}

void ReservationTable::lock(unsigned int slot_offset) {
	// Nothing to do if it's already locked.
	if (isLocked(slot_offset))
		return;
	// Ensure that you *can* lock before actually doing so.
	if (!isIdle(slot_offset))
		throw std::range_error("ReservationTable::lock_bursts for non-idle and non-locked slot.");
	// Then lock.
	slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).setAction(Reservation::LOCKED);
}

bool ReservationTable::canLock(unsigned int slot_offset) const {
	const Reservation& res = slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
	return res.isIdle() || res.isLocked();
}

int32_t ReservationTable::findEarliestOffset(int32_t start_offset, const Reservation& reservation) const {
	for (uint32_t i = start_offset; i < planning_horizon; i++) {
		const Reservation& current_reservation = slot_utilization_vec.at(convertOffsetToIndex(i));
		if (reservation == current_reservation)
			return i;
	}
	throw std::runtime_error("ReservationTable::findEarliestOffset finds no scheduled reservation from present to future.");
}

void ReservationTable::linkFrequencyChannel(FrequencyChannel* channel) {
	this->freq_channel = channel;
}

const FrequencyChannel* ReservationTable::getLinkedChannel() const {
	return freq_channel;
}

const Reservation& ReservationTable::getReservation(int offset) const {
	return getVec().at(convertOffsetToIndex(offset));
}

unsigned long ReservationTable::countReservedTxSlots(const MacId& id) const {
	unsigned long counter = 0;
	for (const Reservation& reservation : slot_utilization_vec)
		if (reservation.getTarget() == id && reservation.isAnyTx())
			counter++;
	return counter;
}

ReservationTable* ReservationTable::getTxReservations(const MacId& id) const {
	auto* table = new ReservationTable(this->planning_horizon);
	for (size_t i = 0; i < slot_utilization_vec.size(); i++) {
		const Reservation& reservation = slot_utilization_vec.at(i);
		if (reservation.getTarget() == id && reservation.isAnyTx())
			table->slot_utilization_vec.at(i) = Reservation(reservation);
	}
	return table;
}

void ReservationTable::integrateTxReservations(const ReservationTable* other) {
	if (other->planning_horizon != this->planning_horizon)
		throw std::invalid_argument("ReservationTable::integrateTxReservations where other table doesn't have the same dimension!");
	for (size_t i = 0; i < slot_utilization_vec.size(); i++) {
		const Reservation& reservation = other->slot_utilization_vec.at(i);
		if (reservation.isAnyTx())
			slot_utilization_vec.at(i) = Reservation(reservation);
	}
}

bool ReservationTable::operator==(const ReservationTable& other) const {
	if (other.planning_horizon != this->planning_horizon || slot_utilization_vec.size() != other.slot_utilization_vec.size())
		return false;
	for (size_t i = 0; i < slot_utilization_vec.size(); i++)
		if (slot_utilization_vec.at(i) != other.slot_utilization_vec.at(i))
			return false;
	return true;
}

bool ReservationTable::operator!=(const ReservationTable& other) const {
	return !((*this) == other);
}

void ReservationTable::linkTransmitterReservationTable(ReservationTable* tx_table) {
	this->transmitter_reservation_table = tx_table;
}

void ReservationTable::linkReceiverReservationTable(ReservationTable* rx_table) {
	this->receiver_reservation_tables.push_back(rx_table);
}

bool ReservationTable::isBurstEnd(int t, const MacId& id) const {
	// If the slot doesn't involve 'id' at all, then it also doesn't end a communication burst.
	if (getReservation(t).getTarget() != id)
		return false;
	else {
		if (getReservation(t + 1).getTarget() == id)
			return false; // If the next slot involves 'id', too, then 't' doesn't end a communication burst.
		else
			return true; // If the next slot does *not* involve 'id', then 't' *does* end a communication burst.
	}
}

ReservationTable::~ReservationTable() = default;
