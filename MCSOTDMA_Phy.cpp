//
// Created by Sebastian Lindner on 10.12.20.
//

#include "MCSOTDMA_Phy.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

MCSOTDMA_Phy::MCSOTDMA_Phy(uint32_t planning_horizon) : transmitter_reservation_table(new ReservationTable(planning_horizon)) {
    // Add *one* P2P receiver.
    receiver_reservation_tables.push_back(new ReservationTable(planning_horizon));
    // The second broadcast receiver is not taken into account here, as we assume it to be always utilized.
}

bool MCSOTDMA_Phy::isTransmitterIdle(unsigned int slot_offset, unsigned int num_slots) const {
	// The transmitter is idle if there's no transmissions scheduled.
	return !transmitter_reservation_table->anyTxReservations(slot_offset, num_slots);
}

bool MCSOTDMA_Phy::isAnyReceiverIdle(unsigned int slot_offset, unsigned int num_slots) const {
    for (const ReservationTable* rx_table : receiver_reservation_tables)
        if (!rx_table->anyRxReservations(slot_offset, num_slots))
            return true;
    return false;
}

MCSOTDMA_Phy::~MCSOTDMA_Phy() {
	delete transmitter_reservation_table;
	for (ReservationTable* rx_table : receiver_reservation_tables)
	    delete rx_table;
}

void MCSOTDMA_Phy::update(uint64_t num_slots) {
	transmitter_reservation_table->update(num_slots);
}

ReservationTable* MCSOTDMA_Phy::getTransmitterReservationTable() {
	return this->transmitter_reservation_table;
}

std::vector<ReservationTable *> MCSOTDMA_Phy::getReceiverReservationTables() {
    return this->receiver_reservation_tables;
}
