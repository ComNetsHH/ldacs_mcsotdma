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

#include <cassert>
#include <sstream>
#include "Reservation.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

Reservation::Reservation(const MacId& target, Reservation::Action action) : target(target), action(action) {}

Reservation::Reservation(const Reservation& other) : target(other.target), action(other.action) {}

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

bool Reservation::isIdle() const {
	return action == IDLE;
}

bool Reservation::isBusy() const {
	return action == BUSY;
}

bool Reservation::isTx() const {
	return action == TX;
}

bool Reservation::isBeaconTx() const {
	return action == TX_BEACON;
}

bool Reservation::isAnyTx() const {
	return isTx() || isBeaconTx();
}

bool Reservation::isRx() const {
	return action == RX;
}

bool Reservation::isBeaconRx() const {
	return action == RX_BEACON;
}

bool Reservation::isAnyRx() const {
	return isRx() || isBeaconRx();
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