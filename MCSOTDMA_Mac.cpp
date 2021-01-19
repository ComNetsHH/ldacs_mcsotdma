//
// Created by Sebastian Lindner on 16.11.20.
//

#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "BCLinkManager.hpp"
#include <IPhy.hpp>
#include <cassert>

using namespace TUHH_INTAIRNET_MCSOTDMA;

MCSOTDMA_Mac::MCSOTDMA_Mac(const MacId& id, uint32_t planning_horizon) : IMac(id), reservation_manager(new ReservationManager(planning_horizon)) {}

MCSOTDMA_Mac::~MCSOTDMA_Mac() {
	for (auto& pair : link_managers)
		delete pair.second;
	delete reservation_manager;
}

void MCSOTDMA_Mac::notifyOutgoing(unsigned long num_bits, const MacId& mac_id) {
	coutd << *this << "::notifyOutgoing(bits=" << num_bits << ", id=" << mac_id << ")... ";
	// Tell the manager of new data.
	getLinkManager(mac_id)->notifyOutgoing(num_bits);
}

void MCSOTDMA_Mac::passToLower(L2Packet* packet, unsigned int center_frequency) {
	assert(lower_layer && "MCSOTDMA_Mac's lower layer is unset.");
	lower_layer->receiveFromUpper(packet, center_frequency);
}

void MCSOTDMA_Mac::passToUpper(L2Packet* packet) {
	assert(upper_layer && "MCSOTDMA_Mac::passToUpper upper layer is not set.");
	upper_layer->receiveFromLower(packet);
}

void MCSOTDMA_Mac::update(int64_t num_slots) {
	// Update time.
	IMac::update(num_slots);
	coutd << *this << "::update(" << num_slots << ")... ";
	// Notify the broadcast channel manager.
	auto* bc_link_manager = (BCLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
	bc_link_manager->update(num_slots);
	// Notfy all other LinkManagers.
	for (auto item : link_managers)
	    item.second->update(num_slots);
	// Notify the ReservationManager.
	assert(reservation_manager && "MCSOTDMA_MAC::update with unset ReserationManager.");
	reservation_manager->update(num_slots);
}

void MCSOTDMA_Mac::receiveFromLower(L2Packet* packet) {
	const MacId& dest_id = packet->getDestination();
	coutd << *this << "::receiveFromLower(" << dest_id << ")... ";
	if (dest_id == SYMBOLIC_ID_UNSET)
		throw std::invalid_argument("MCSOTDMA_Mac::receiveFromLower for unset dest_id.");
	// Forward broadcasts to the BCLinkManager...
	if (dest_id == SYMBOLIC_LINK_ID_BROADCAST || dest_id == SYMBOLIC_LINK_ID_BEACON)
		getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->receiveFromLower(packet);
	// unicasts intended for us to the corresponding LinkManager that manages the packet's sender
	else if (dest_id == id)
		getLinkManager(packet->getOrigin())->receiveFromLower(packet);
	else
		coutd << "packet not intended for us; discarding." << std::endl;
}

LinkManager* MCSOTDMA_Mac::getLinkManager(const MacId& id) {
	if (id == getMacId()) {
		throw std::invalid_argument("MCSOTDMA_Mac::getLinkManager for own MAC ID.");
	}
	// Beacon should be treated like Broadcast.
	MacId internal_id = MacId(id);
	if (internal_id == SYMBOLIC_LINK_ID_BEACON)
		internal_id = SYMBOLIC_LINK_ID_BROADCAST;
	
	// Look for an existing link manager...
	auto it = link_managers.find(internal_id);
	LinkManager* link_manager;
	// ... if there already is one ...
	if (it != link_managers.end()) {
		link_manager = (*it).second;
//		coutd << "found existing LinkManager(" << internal_id << ") ";
	// ... if there's none ...
	} else {
		// Auto-assign broadcast channel
		if (internal_id == SYMBOLIC_LINK_ID_BROADCAST) {
			link_manager = new BCLinkManager(internal_id, reservation_manager, this);
			link_manager->assign(reservation_manager->getBroadcastFreqChannel());
		} else
			link_manager = new LinkManager(internal_id, reservation_manager, this);
		auto insertion_result = link_managers.insert(std::map<MacId, LinkManager*>::value_type(internal_id, link_manager));
		if (!insertion_result.second)
			throw std::runtime_error("Attempted to insert new LinkManager, but there already was one.");
//		coutd << "instantiated new " << (internal_id == SYMBOLIC_LINK_ID_BROADCAST ? "BCLinkManager" : "LinkManager") << "(" << internal_id << ") ";
		
	}
	return link_manager;
}

void MCSOTDMA_Mac::forwardLinkReply(L2Packet* reply, const FrequencyChannel* channel, int32_t slot_offset, unsigned int timeout, unsigned int offset, unsigned int length) {
    coutd << *this << "::forwardLinkReply(to=" << reply->getDestination() << ") ";
	LinkManager* manager = getLinkManager(reply->getDestination());
	manager->assign(channel);
	manager->scheduleLinkReply(reply, slot_offset, timeout, offset, length);
}

std::pair<size_t, size_t> MCSOTDMA_Mac::execute() {
	// Fetch all reservations of the current time slot.
	std::vector<std::pair<Reservation, const FrequencyChannel*>> reservations = reservation_manager->collectCurrentReservations();
	coutd << *this << " processing " << reservations.size() << " reservations..." << std::endl;
	coutd.increaseIndent();
	size_t num_txs = 0, num_rxs = 0;
	for (const std::pair<Reservation, const FrequencyChannel*>& pair : reservations) {
		const Reservation& reservation = pair.first;
		const FrequencyChannel* channel = pair.second;

        coutd << reservation << std::endl;
        coutd.increaseIndent();
		switch (reservation.getAction()) {
			case Reservation::IDLE: {
				// No user is utilizing this slot.
				// Nothing to do.
				break;
			}
			case Reservation::BUSY: {
				// Some other user is utilizing this slot.
				// Nothing to do.
				break;
			}
			case Reservation::TX_CONT: {
				// Transmission has already started, so a transmitter is busy.
				num_txs++;
				// Nothing else to do.
				break;
			}
			case Reservation::RX: {
				// Ensure that we have not too many receptions scheduled.
				num_rxs++;
				if (num_rxs > num_receivers)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many receptions within this time slot.");
				// Tune the receiver.
				onReceptionSlot(channel);
				break;
			}
			case Reservation::TX: {
				// Ensure that we have no simultaneous transmissions scheduled.
				num_txs++;
				if (num_txs > num_transmitters)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many transmissions within this time slot.");
				// Find the corresponding LinkManager.
				const MacId& id = reservation.getTarget();
				LinkManager* link_manager;
				try {
					link_manager = link_managers.at(id);
				} catch (const std::exception& e) {
					throw std::runtime_error(
							"MCSOTDMA_Mac::execute caught exception while looking for the corresponding LinkManager: " +
							std::string(e.what()));
				}
				// Tell it about the transmission slot.
				unsigned int num_tx_slots = reservation.getNumRemainingSlots() + 1;
				L2Packet* outgoing_packet = link_manager->onTransmissionSlot(num_tx_slots);
				outgoing_packet->notifyCallbacks();
				passToLower(outgoing_packet, channel->getCenterFrequency());
				break;
			}
		}
		coutd.decreaseIndent();
		coutd << std::endl;
	}
	coutd.decreaseIndent();
	return {num_txs, num_rxs};
}
