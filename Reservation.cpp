//
// Created by Sebastian Lindner on 04.11.20.
//

#include "Reservation.hpp"

TUHH_INTAIRNET_MCSOTDMA::Reservation::Reservation(const TUHH_INTAIRNET_MCSOTDMA::MacId& owner,
                                                  TUHH_INTAIRNET_MCSOTDMA::Reservation::Action action)
                                                  : owner(owner), action(action) {}

const TUHH_INTAIRNET_MCSOTDMA::MacId& TUHH_INTAIRNET_MCSOTDMA::Reservation::getOwner() const {
	return this->owner;
}

const TUHH_INTAIRNET_MCSOTDMA::Reservation::Action& TUHH_INTAIRNET_MCSOTDMA::Reservation::getAction() const {
	return this->action;
}

TUHH_INTAIRNET_MCSOTDMA::Reservation::Reservation(const TUHH_INTAIRNET_MCSOTDMA::MacId& owner) : owner(owner), action(Action::IDLE) {}

void TUHH_INTAIRNET_MCSOTDMA::Reservation::setAction(TUHH_INTAIRNET_MCSOTDMA::Reservation::Action action) {
	this->action = action;
}

TUHH_INTAIRNET_MCSOTDMA::Reservation::Reservation() : Reservation(SYMBOLIC_ID_UNSET) {}

TUHH_INTAIRNET_MCSOTDMA::Reservation::~Reservation() = default;

bool TUHH_INTAIRNET_MCSOTDMA::Reservation::operator==(const TUHH_INTAIRNET_MCSOTDMA::Reservation& other) const {
	return other.action == this->action && other.owner == this->owner;
}

bool TUHH_INTAIRNET_MCSOTDMA::Reservation::operator!=(const TUHH_INTAIRNET_MCSOTDMA::Reservation& other) const {
	return !(*this == other);
}
