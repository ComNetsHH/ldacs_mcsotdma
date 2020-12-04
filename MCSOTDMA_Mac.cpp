//
// Created by Sebastian Lindner on 16.11.20.
//

#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include <IPhy.hpp>
#include <cassert>

using namespace TUHH_INTAIRNET_MCSOTDMA;

MCSOTDMA_Mac::MCSOTDMA_Mac(ReservationManager* reservation_manager)	: reservation_manager(reservation_manager) {}

MCSOTDMA_Mac::~MCSOTDMA_Mac() {
	for (auto& pair : link_managers)
		delete pair.second;
}

void MCSOTDMA_Mac::notifyOutgoing(unsigned long num_bits, const MacId& mac_id) {
	coutd << "MAC::notifyOutgoing(" << num_bits << ", " << mac_id.getId() << ")... ";
	// Look for an existing link manager...
	auto it = link_managers.find(mac_id);
	LinkManager* link_manager;
	// ... if there's none ...
	if (it == link_managers.end()) {
		link_manager = new LinkManager(mac_id, reservation_manager, this);
		auto insertion_result = link_managers.insert(std::map<MacId, LinkManager*>::value_type(mac_id, link_manager));
		if (!insertion_result.second)
			throw std::runtime_error("Attempted to insert new LinkManager, but there already was one.");
		coutd << "created new LinkManager." << std::endl;
	// ... if there already is one ...
	} else {
		link_manager = (*it).second;
		coutd << "found existing LinkManager." << std::endl;
	}
	
	// Tell the manager of new data.
	link_manager->notifyOutgoing(num_bits);
}

void MCSOTDMA_Mac::passToLower(L2Packet* packet, unsigned int center_frequency) {
	assert(lower_layer && "MCSOTDMA_Mac's lower layer is unset.");
	lower_layer->receiveFromUpper(packet, center_frequency);
}

bool MCSOTDMA_Mac::shouldLinkBeArqProtected(const MacId& mac_id) const {
	assert(upper_layer && "MCSOTDMA_Mac's upper layer is unset.");
	return this->upper_layer->shouldLinkBeArqProtected(mac_id);
}

unsigned long MCSOTDMA_Mac::getCurrentDatarate() const {
	assert(lower_layer && "MCSOTDMA_Mac::getCurrentDatarate for unset PHY layer.");
	return lower_layer->getCurrentDatarate();
}

LinkManager* MCSOTDMA_Mac::getLinkManager(const MacId& id) {
	return link_managers.at(id);
}

void MCSOTDMA_Mac::passToUpper(L2Packet* packet) {
	assert(upper_layer && "MCSOTDMA_Mac::passToUpper upper layer is not set.");
	upper_layer->receiveFromLower(packet);
}

void MCSOTDMA_Mac::update(uint64_t num_slots) {
	coutd << "MAC::update(" << num_slots << ")... ";
	// Notify the ReservationManager.
	assert(reservation_manager && "MCSOTDMA_MAC::update with unset ReserationManager.");
	reservation_manager->update(num_slots);
	// Now the reservation tables have been updated.
	// Fetch all reservations of the current time slot.
	std::vector<std::pair<Reservation, const FrequencyChannel*>> reservations = reservation_manager->collectCurrentReservations();
	coutd << "processing " << reservations.size() << " reservations..." << std::endl;
	size_t num_txs = 0, num_rxs = 0;
	for (const std::pair<Reservation, const FrequencyChannel*>& pair : reservations) {
		const Reservation& reservation = pair.first;
		const FrequencyChannel* channel = pair.second;
		coutd << "\t" << reservation << std::endl;
		
		switch (reservation.getAction()) {
			case Reservation::IDLE:
				// No user is utilizing this slot.
				// Nothing to do.
				break;
			case Reservation::BUSY:
				// Some other user is utilizing this slot.
				// Nothing to do.
				break;
			case Reservation::RX:
				// Ensure that we have not too many receptions scheduled.
				num_rxs++;
				if (num_rxs > num_receivers)
					throw std::runtime_error("MCSOTDMA_Mac::update for too many receptions within this time slot.");
				// Tune the receiver.
				onReceptionSlot(channel);
				break;
			case Reservation::TX:
				// Ensure that we have no simultaneous transmissions scheduled.
				num_txs++;
				if (num_txs > num_transmitters)
					throw std::runtime_error("MCSOTDMA_Mac::update for too many transmissions within this time slot.");
				// Find the corresponding LinkManager.
				const MacId& id = reservation.getOwner();
				LinkManager* link_manager;
				try {
					link_manager = link_managers.at(id);
				} catch (const std::exception& e) {
					throw std::runtime_error("MCSOTDMA_Mac::update caught exception while looking for the corresponding LinkManager: " + std::string(e.what()));
				}
				// Tell it about the transmission slot.
				link_manager->onTransmissionSlot();
				break;
		}
	}
}
