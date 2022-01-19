//
// Created by Sebastian Lindner on 04.11.20.
//

#include <cassert>
#include <sstream>
#include "Reservation.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

Reservation::Reservation(const MacId& target, Reservation::Action action, unsigned int num_remaining_slots) : target(target), action(action), num_remaining_slots(num_remaining_slots) {}

Reservation::Reservation(const MacId& target, Reservation::Action action) : Reservation(target, action, 0) {}

Reservation::Reservation(const Reservation& other) : target(other.target), action(other.action), num_remaining_slots(other.num_remaining_slots) {}

Reservation::Reservation() : Reservation(SYMBOLIC_ID_UNSET) {}

Reservation::Reservation(const MacId& target) : Reservation(target, Action::IDLE) {}

Reservation::~Reservation() = default;

const MacId& Reservation::getTarget() const {
	return this->target;
}

void Reservation::setTarget(const MacId& target) {
	this->target = target;
}

const Reservation::Action& Reservation::getAction() const {
	return this->action;
}

void Reservation::setAction(Reservation::Action action) {
	this->action = action;
}

bool Reservation::operator==(const Reservation& other) const {
	return other.action == this->action && other.target == this->target;
}

bool Reservation::operator!=(const Reservation& other) const {
	return !(*this == other);
}

unsigned int Reservation::getNumRemainingSlots() const {
	return this->num_remaining_slots;
}

void Reservation::setNumRemainingSlots(const unsigned int& num_slots) {
	this->num_remaining_slots = num_slots;
}

bool Reservation::isIdle() const {
	return action == IDLE;
}

bool Reservation::isBusy() const {
	return action == BUSY;
}

bool Reservation::isTx() const {
	return action == TX;
}

bool Reservation::isTxCont() const {
	return action == TX_CONT;
}

bool Reservation::isBeaconTx() const {
	return action == TX_BEACON;
}

bool Reservation::isAnyTx() const {
	return isTx() || isTxCont() || isBeaconTx();
}

bool Reservation::isRx() const {
	return action == RX;
}

bool Reservation::isRxCont() const {
	return action == RX_CONT;
}

bool Reservation::isBeaconRx() const {
	return action == RX_BEACON;
}

bool Reservation::isAnyRx() const {
	return isRx() || isRxCont() || isBeaconRx();
}

bool Reservation::isBeacon() const {
	return isBeaconRx() || isBeaconTx();
}

bool Reservation::isLocked() const {
	return action == LOCKED;
}

std::string Reservation::toString() const {
	std::stringstream stream;
	stream << *this;
	return stream.str();
}

//bool Reservation::lock() {
//	if (!isIdle())
//		return false;
//	action = LOCKED;
//	return true;
//}

