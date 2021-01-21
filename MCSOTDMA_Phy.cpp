//
// Created by Sebastian Lindner on 10.12.20.
//

#include "MCSOTDMA_Phy.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

MCSOTDMA_Phy::MCSOTDMA_Phy(uint32_t planning_horizon) : transmitter_reservations(new ReservationTable(planning_horizon)) {
    // Add two receivers.
    receiver_reservations.push_back(new ReservationTable(planning_horizon));
    receiver_reservations.push_back(new ReservationTable(planning_horizon));
}

bool MCSOTDMA_Phy::isTransmitterIdle(unsigned int slot_offset, unsigned int num_slots) const {
	// The transmitter is idle if there's no transmissions scheduled.
	return !transmitter_reservations->anyTxReservations(slot_offset, num_slots);
}

bool MCSOTDMA_Phy::isAnyReceiverIdle(unsigned int slot_offset, unsigned int num_slots) const {
    for (const ReservationTable* rx_table : receiver_reservations)
        if (!rx_table->anyRxReservations(slot_offset, num_slots))
            return true;
    return false;
}

MCSOTDMA_Phy::~MCSOTDMA_Phy() {
	delete transmitter_reservations;
	for (ReservationTable* rx_table : receiver_reservations)
	    delete rx_table;
}

void MCSOTDMA_Phy::update(uint64_t num_slots) {
	transmitter_reservations->update(num_slots);
}

ReservationTable* MCSOTDMA_Phy::getTransmitterReservationTable() {
	return this->transmitter_reservations;
}
