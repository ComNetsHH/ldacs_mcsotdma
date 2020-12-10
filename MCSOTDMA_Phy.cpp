//
// Created by Sebastian Lindner on 10.12.20.
//

#include "MCSOTDMA_Phy.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

MCSOTDMA_Phy::MCSOTDMA_Phy(uint32_t planning_horizon) : transmitter_reservations(new ReservationTable(planning_horizon)) {}

bool MCSOTDMA_Phy::isTransmitterIdle(unsigned int slot_offset, unsigned int num_slots) const {
	// The transmitter is idle if there's no transmissions scheduled.
	return !transmitter_reservations->anyTxReservations(slot_offset, num_slots);
}

MCSOTDMA_Phy::~MCSOTDMA_Phy() {
	delete transmitter_reservations;
}

void MCSOTDMA_Phy::update(uint64_t num_slots) {
	transmitter_reservations->update(num_slots);
}

ReservationTable* MCSOTDMA_Phy::getTransmitterReservationTable() {
	return this->transmitter_reservations;
}
