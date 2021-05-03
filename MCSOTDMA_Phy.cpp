//
// Created by Sebastian Lindner on 10.12.20.
//

#include "MCSOTDMA_Phy.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

MCSOTDMA_Phy::MCSOTDMA_Phy(uint32_t planning_horizon) : transmitter_reservation_table(new ReservationTable(planning_horizon)) {
	// Add one P2P receiver.
	receiver_reservation_tables.push_back(new ReservationTable(planning_horizon));
	// Don't add a BC receiver. This is assumed as always busy.
//    receiver_reservation_tables.push_back(new ReservationTable(planning_horizon, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::RX)));
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
	// Clear RX frequencies.
	IPhy::update(num_slots);
	// Update reservation tables.
	transmitter_reservation_table->update(num_slots);
	for (auto* rx_table : receiver_reservation_tables)
		rx_table->update(num_slots);
}

ReservationTable* MCSOTDMA_Phy::getTransmitterReservationTable() {
	return this->transmitter_reservation_table;
}

std::vector<ReservationTable*>& MCSOTDMA_Phy::getReceiverReservationTables() {
	return this->receiver_reservation_tables;
}

void MCSOTDMA_Phy::onReception(L2Packet* packet, uint64_t center_frequency) {
	// Make sure a receiver is tuned to this channel at the moment.
	if (std::any_of(rx_frequencies.begin(), rx_frequencies.end(), [center_frequency](uint64_t rx_freq) {
		return center_frequency == rx_freq;
	})) {
		coutd << "PHY receives packet -> ";
		statistic_num_received_packets++;
		emit(str_statistic_num_received_packets, statistic_num_received_packets);

		IPhy::onReception(packet, center_frequency);
	} else {
		coutd << "PHY doesn't receive packet (no RX tuned to frequency '" << center_frequency << "kHz').";
		if (packet->getDestination() == SYMBOLIC_LINK_ID_BROADCAST || packet->getDestination() == ((MCSOTDMA_Mac*) upper_layer)->getMacId()) {
			statistic_num_missed_packets++;
			emit(str_statistic_num_missed_packets, statistic_num_missed_packets);
			coutd << " (this was destined to us, so I'm counting it as a missed packet).";
		}
		coutd << std::endl;
	}
}