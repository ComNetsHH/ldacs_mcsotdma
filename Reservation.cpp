//
// Created by Sebastian Lindner on 04.11.20.
//

#include <cassert>
#include "Reservation.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

Reservation::Reservation(const MacId& owner, Reservation::Action action, unsigned int num_remaining_tx_slots) : owner(owner), action(action), num_remaining_tx_slots(num_remaining_tx_slots) {
	assert(num_remaining_tx_slots == 0 || (action == Action::TX || action == Action::TX_CONT) && "A multi-slot reservation must be for transmission actions.");
}

Reservation::Reservation(const MacId& owner, Reservation::Action action) : Reservation(owner, action, 0) {}

Reservation::Reservation(const Reservation& other) : owner(other.owner), action(other.action), num_remaining_tx_slots(other.num_remaining_tx_slots) {}

Reservation::Reservation() : Reservation(SYMBOLIC_ID_UNSET) {}

Reservation::Reservation(const MacId& owner) : Reservation(owner, Action::IDLE) {}

Reservation::~Reservation() = default;

const MacId& Reservation::getOwner() const {
	return this->owner;
}

const Reservation::Action& Reservation::getAction() const {
	return this->action;
}

void Reservation::setAction(Reservation::Action action) {
	this->action = action;
}

bool Reservation::operator==(const Reservation& other) const {
	return other.action == this->action && other.owner == this->owner;
}

bool Reservation::operator!=(const Reservation& other) const {
	return !(*this == other);
}

unsigned int Reservation::getNumRemainingTxSlots() const {
	return this->num_remaining_tx_slots;
}

void Reservation::setNumRemainingTxSlots(const unsigned int& num_slots) {
	assert(action == Action::TX || action == Action::TX_CONT);
	this->num_remaining_tx_slots = num_slots;
}

