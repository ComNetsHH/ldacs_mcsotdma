//
// Created by Sebastian Lindner on 06.10.20.
//

#include <stdexcept>
#include <algorithm>
#include <math.h>
#include <limits>
#include <iostream>
#include "ReservationTable.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ReservationTable::ReservationTable(uint32_t planning_horizon)
	: planning_horizon(planning_horizon), slot_utilization_vec(std::vector<Reservation>(uint64_t(planning_horizon * 2 + 1))), last_updated(), num_idle_future_slots(planning_horizon + 1) {
	// The planning horizon denotes how many slots we want to be able to look into future and past.
	// Since the current moment in time must also be represented, we need planning_horizon*2+1 values.
	// If we use UINT32_MAX, then we wouldn't be able to store 2*UINT32_MAX+1 in UINT64, so throw an exception if this is attempted.
	if (planning_horizon == UINT32_MAX)
		throw std::invalid_argument("Cannot instantiate a reservation table with a planning horizon of UINT32_MAX. It must be at least one slot less.");
	// In practice, allocating even much less results in a std::bad_alloc anyway...
}

uint32_t ReservationTable::getPlanningHorizon() const {
	return this->planning_horizon;
}

Reservation* ReservationTable::mark(int32_t slot_offset, const Reservation& reservation) {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::mark planning_horizon=" + std::to_string(planning_horizon) + " smaller than queried slot_offset=" + std::to_string(slot_offset) + "!");
	bool currently_idle = this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).isIdle();
	this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)) = reservation;
	// Update the number of idle slots.
	if (!reservation.isIdle())
		num_idle_future_slots--;
	else if (!currently_idle) // changing from non-idle to idle
		num_idle_future_slots++;
	// If a transmitter table is linked, mark it there, too.
	if (transmitter_reservation_table != nullptr && (reservation.getAction() == Reservation::TX || reservation.getAction() == Reservation::TX_CONT))
		transmitter_reservation_table->mark(slot_offset, reservation);
	// Same for receiver tables
	if (!receiver_reservation_tables.empty() && reservation.getAction() == Reservation::RX)
	    for (ReservationTable* rx_table : receiver_reservation_tables)
	        rx_table->mark(slot_offset, reservation);
	// If this is a multi-slot transmission reservation, set the following ones, too.
	if (reservation.getNumRemainingSlots() > 0) {
		Reservation::Action action = reservation.getAction();
		if (action == Reservation::TX)
			action = Reservation::TX_CONT;
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

bool ReservationTable::anyTxReservations(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::anyTxReservations for planning horizon smaller than queried offset!");
	return this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).isTx()
		|| this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).isTxCont();
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
    return this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).isRx();
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

int32_t ReservationTable::findEarliestIdleRange(int32_t start, uint32_t length, bool consider_transmitter, bool consider_receivers) const {
    if (!isValid(start, length))
        throw std::invalid_argument("Invalid slot range!");
	if (consider_transmitter && transmitter_reservation_table == nullptr)
		throw std::runtime_error("ReservationTable::findEarliestIdleRange with consider_transmitter==true for unset transmitter table.");
	if (consider_receivers && receiver_reservation_tables.empty())
        throw std::runtime_error("ReservationTable::findEarliestIdleRange with consider_receivers==true for unset receiver tables.");
	bool idle = false, tx_idle = false, rx_idle = false;
	for (int32_t i = start; i < int32_t(this->planning_horizon); i++) {
		if (this->isIdle(i, length)) {
		    // Neither TX nor RX matter
		    if (!consider_transmitter && !consider_receivers)
		        return i;
		    // Both RX && TX matter
		    else if (consider_transmitter && consider_receivers) {
                tx_idle = !transmitter_reservation_table->isIdle(i, length);
                rx_idle = std::any_of(receiver_reservation_tables.begin(), receiver_reservation_tables.end(), [start, length](ReservationTable* table) {
                        return table->isIdle(start, length);
                        });
                return tx_idle && rx_idle;
            // TX && !RX
		    } else if (consider_transmitter && !consider_receivers) {
                if (transmitter_reservation_table->isIdle(i, length))
                    return i;
            // !TX && RX
            } else {
                rx_idle = std::any_of(receiver_reservation_tables.begin(), receiver_reservation_tables.end(), [i, length](ReservationTable *table) {
                    return table->isIdle(i, length);
                });
                if (rx_idle)
                    return i;
            }
		}
	}
    throw std::range_error("No idle slot range of specified length found.");
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
	return abs(slot_offset) <= this->planning_horizon; // can't move more than one horizon into either direction of time.
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
	for (auto it = slot_utilization_vec.begin(); it < slot_utilization_vec.begin() + num_slots; it++) {
		Reservation reservation = *it;
		if (!reservation.isIdle())
			num_busy_slots++;
	}
	num_idle_future_slots += num_busy_slots; // As these go out of scope, we may have more idle slots now.
	
	// Shift all elements to the front, old ones are overwritten.
	std::move(this->slot_utilization_vec.begin() + num_slots, this->slot_utilization_vec.end(), this->slot_utilization_vec.begin());
	// All new elements are initialized as idle.
	for (auto it = slot_utilization_vec.end() - 1; it >= slot_utilization_vec.end() - num_slots; it--)
		*it = Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::IDLE);
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

std::vector<int32_t> ReservationTable::findCandidateSlots(unsigned int min_offset, unsigned int num_candidates, unsigned int range_length, bool consider_transmitter, bool consider_receivers) const {
	std::vector<int32_t> start_slots;
	int32_t last_offset = min_offset;
	for (size_t i = 0; i < num_candidates; i++) {
		// Try to find another slot range.
		try {
			int32_t start_slot = findEarliestIdleRange(last_offset, range_length, consider_transmitter, consider_receivers);
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

bool ReservationTable::lock(const std::vector<int32_t>& slot_offsets, bool lock_rx, bool lock_tx) {
	for (int32_t t : slot_offsets) {
		if (!slot_utilization_vec.at(convertOffsetToIndex(t)).lock())
			return false;
		if (lock_tx) {
		    if (transmitter_reservation_table == nullptr)
		        throw std::runtime_error("ReservationTable::lock with lock_tx=true and unset transmitter reservation table.");
		    if (!transmitter_reservation_table->lock(slot_offsets, false, false))
		        return false;
		}
		if (lock_rx) {
		    if (receiver_reservation_tables.empty())
                throw std::runtime_error("ReservationTable::lock with lock_rx=true and unset receiver reservation table.");
		    bool succeeded = false;
		    for (ReservationTable* rx_table : receiver_reservation_tables) {
		        if (rx_table->lock(slot_offsets, false, false)) {
		            succeeded = true;
                    break;
                }
		    }
		    if (!succeeded)
		        return false;
		}
	}
	return true;
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
		if (reservation.getTarget() == id && (reservation.isTx() || reservation.isTxCont()))
			counter++;
	return counter;
}

ReservationTable* ReservationTable::getTxReservations(const MacId& id) const {
	auto* table = new ReservationTable(this->planning_horizon);
	for (size_t i = 0; i < slot_utilization_vec.size(); i++) {
		const Reservation& reservation = slot_utilization_vec.at(i);
		if (reservation.getTarget() == id && (reservation.isTx() || reservation.isTxCont()))
			table->slot_utilization_vec.at(i) = Reservation(reservation);
	}
	return table;
}

void ReservationTable::integrateTxReservations(const ReservationTable* other) {
	if (other->planning_horizon != this->planning_horizon)
		throw std::invalid_argument("ReservationTable::integrateTxReservations where other table doesn't have the same dimension!");
	for (size_t i = 0; i < slot_utilization_vec.size(); i++) {
		const Reservation& reservation = other->slot_utilization_vec.at(i);
		if (reservation.isTx() || reservation.isTxCont())
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

ReservationTable::~ReservationTable() = default;
