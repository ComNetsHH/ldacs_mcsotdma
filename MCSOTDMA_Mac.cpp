//
// Created by Sebastian Lindner on 16.11.20.
//

#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "InetPacketPayload.hpp"
#include "P2PLinkManager.hpp"
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

void MCSOTDMA_Mac::update(uint64_t num_slots) {
	// Update time.
	IMac::update(num_slots);
	coutd << "t=" << getCurrentSlot() << " " << *this << "::onSlotStart(" << num_slots << ")... ";
	// Notify the ReservationManager.
	assert(reservation_manager && "MCSOTDMA_MAC::onSlotStart with unset ReserationManager.");
	reservation_manager->update(num_slots);
	// Notify PHY.
	assert(lower_layer && "IMac::onSlotStart for unset lower layer.");
	lower_layer->update(num_slots);
	// Notify the broadcast channel manager.
	getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onSlotStart(num_slots);
	// Notify all other LinkManagers.
	for (auto item : link_managers) {
		if (item.first != SYMBOLIC_LINK_ID_BROADCAST)
			item.second->onSlotStart(num_slots);
	}
	// Notify the PHY about the channels to which receivers are tuned to in this time slot.
	std::vector<std::pair<Reservation, const FrequencyChannel*>> reservations = reservation_manager->collectCurrentReservations();
	size_t num_rx = 0;
	for (const auto& pair : reservations) {
		if (pair.first.isAnyRx()) {
			num_rx++;
			try {
				lower_layer->tuneReceiver(pair.second->getCenterFrequency());
			} catch (const std::runtime_error& e) {
				throw std::runtime_error("MCSOTDMA(" + std::to_string(id.getId()) + ")::onSlotStart(" + std::to_string(num_slots) + ") couldn't tune receiver for " + std::to_string(num_rx) + " RX reservations.");
			}
		}
	}
	coutd << std::endl;
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

		coutd << *channel << ":" << reservation << std::endl;
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
			case Reservation::RX: {
				// Ensure that we have not too many receptions scheduled.
				num_rxs++;
				if (num_rxs > num_receivers)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many receptions within this time slot.");
				LinkManager *link_manager = getLinkManager(reservation.getTarget());
				link_manager->onReceptionBurstStart(reservation.getNumRemainingSlots());
				onReceptionSlot(channel);
				break;
			}
			case Reservation::RX_CONT: {
				num_rxs++;
				if (num_rxs > num_receivers)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many receptions within this time slot.");
				LinkManager *link_manager = getLinkManager(reservation.getTarget());
				link_manager->onReceptionBurst(reservation.getNumRemainingSlots());
				onReceptionSlot(channel);
				break;
			}
			case Reservation::TX: {
				// Ensure that we have no simultaneous transmissions scheduled.
				num_txs++;
				if (num_txs > num_transmitters)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many transmissions within this time slot.");
				// Find the corresponding OldLinkManager.
				const MacId& id = reservation.getTarget();
				LinkManager* link_manager = getLinkManager(id);
				// Tell it about the transmission slot.
				unsigned int num_tx_slots = reservation.getNumRemainingSlots();
				L2Packet* outgoing_packet = link_manager->onTransmissionBurstStart(num_tx_slots);
				outgoing_packet->notifyCallbacks();
				passToLower(outgoing_packet, channel->getCenterFrequency());
				break;
			}
			case Reservation::TX_CONT: {
				// Transmission has already started, so a transmitter is busy.
				num_txs++;
				if (num_txs > num_transmitters)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many transmissions within this time slot.");
				LinkManager *link_manager = getLinkManager(reservation.getTarget());
				link_manager->onTransmissionBurst(reservation.getNumRemainingSlots());
				break;
			}
			case Reservation::TX_BEACON: {
				num_txs++;
				if (num_txs > num_transmitters)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many transmissions within this time slot.");
				passToLower(getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onTransmissionBurstStart(reservation.getNumRemainingSlots()), channel->getCenterFrequency());
			}
			case Reservation::RX_BEACON: {
				num_rxs++;
				if (num_rxs > num_transmitters)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many transmissions within this time slot.");
				getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onReceptionBurstStart(reservation.getNumRemainingSlots());
			}
		}
		coutd.decreaseIndent();
		coutd << std::endl;
	}
	coutd.decreaseIndent();
	return {num_txs, num_rxs};
}

void MCSOTDMA_Mac::receiveFromLower(L2Packet* packet, uint64_t center_frequency) {
	const MacId& dest_id = packet->getDestination();
	coutd << *this << "::onPacketReception(from=" << packet->getOrigin() << ", to=" << dest_id << ", f=" << center_frequency << "kHz)... ";
	if (dest_id == SYMBOLIC_ID_UNSET)
		throw std::invalid_argument("MCSOTDMA_Mac::onPacketReception for unset dest_id.");
	statistic_num_packets_received++;
	// Store,
	if (dest_id == SYMBOLIC_LINK_ID_BROADCAST || dest_id == SYMBOLIC_LINK_ID_BEACON || dest_id == id) {
		received_packets[center_frequency].push_back(packet);
		coutd << "stored until slot end.";
	// ... or discard.
	} else
		coutd << "packet not intended for us; discarding." << std::endl;
}

LinkManager* MCSOTDMA_Mac::getLinkManager(const MacId& id) {
	if (id == getMacId())
		throw std::invalid_argument("MCSOTDMA_Mac::getLinkManager for own MAC ID.");
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
		// ... if there's none ...
	} else {
		// Auto-assign broadcast channel
		if (internal_id == SYMBOLIC_LINK_ID_BROADCAST) {
			link_manager = new BCLinkManager(reservation_manager, this, 1);
			link_manager->assign(reservation_manager->getBroadcastFreqChannel());
		} else {
			link_manager = new P2PLinkManager(internal_id, reservation_manager, this, 10, 15);
			// Receiver tables are only set for P2PLinkManagers.
			for (ReservationTable* rx_table : reservation_manager->getRxTables())
				link_manager->linkRxTable(rx_table);
		}
		link_manager->linkTxTable(reservation_manager->getTxTable());
		auto insertion_result = link_managers.insert(std::map<MacId, LinkManager*>::value_type(internal_id, link_manager));
		if (!insertion_result.second)
			throw std::runtime_error("Attempted to insert new LinkManager, but there already was one.");

	}
	return link_manager;
}

void MCSOTDMA_Mac::onReceptionSlot(const FrequencyChannel* channel) {
	// Do nothing.
}

ReservationManager* MCSOTDMA_Mac::getReservationManager() {
    return this->reservation_manager;
}

void MCSOTDMA_Mac::onSlotEnd() {
	for (auto &packet_freq_pair : received_packets) {
		// On this frequency channel,
		uint64_t freq = packet_freq_pair.first;
		// these packets were received.
		std::vector<L2Packet*> packets = packet_freq_pair.second;

		// If it's just a single packet, then it can be decoded.
		if (packets.size() == 1) {
			L2Packet *packet = packets.at(0);
			if (packet->getDestination() == SYMBOLIC_LINK_ID_BROADCAST || packet->getDestination() == SYMBOLIC_LINK_ID_BEACON)
				getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onPacketReception(packet);
			else
				getLinkManager(packet->getOrigin())->onPacketReception(packet);
			statistic_num_packet_decoded++;
		// We cannot receive several packets on this channel simultaneously - drop it due to a collision.
		} else if (packets.size() > 1) {
			coutd << *this << " collision on frequency " << freq << " -> dropping " << packets.size() << " packets.";
			statistic_num_packet_collisions += packets.size();

            for (auto *packet : packets) {
                auto payloads = packet->getPayloads();
                auto headers = packet->getHeaders();
                for (int i = 0; i< headers.size(); i++) {
                    if(payloads[i]) {
                        if(headers[i]->frame_type == L2Header::FrameType::broadcast || headers[i]->frame_type == L2Header::FrameType::unicast) {
                            if(((InetPacketPayload*)payloads[i])->original != nullptr) {
                                //delete ((InetPacketPayload*)payloads[i])->original;
                                //((InetPacketPayload*)payloads[i])->original = nullptr;
                            }
                        }
                    }
                }
                delete packet;
            }

        }
	}
	received_packets.clear();

	for (auto item : link_managers)
		item.second->onSlotEnd();

	// Statistics reporting.
	emit(str_statistic_num_packets_received, statistic_num_packets_received);
	emit(str_statistic_num_broadcasts_received, statistic_num_broadcasts_received);
	emit(str_statistic_num_unicasts_received, statistic_num_unicasts_received);
	emit(str_statistic_num_packet_collisions, statistic_num_packet_collisions);
	emit(str_statistic_num_packet_decoded, statistic_num_packet_decoded);
	emit(str_statistic_num_requests_received, statistic_num_requests_received);
	emit(str_statistic_num_replies_received, statistic_num_replies_received);
	emit(str_statistic_num_beacons_received, statistic_num_beacons_received);
	emit(str_statistic_num_link_infos_received, statistic_num_link_infos_received);
	emit(str_statistic_num_packets_sent, statistic_num_packets_sent);
	emit(str_statistic_num_broadcasts_sent, statistic_num_broadcasts_sent);
	emit(str_statistic_num_unicasts_sent, statistic_num_unicasts_sent);
	emit(str_statistic_num_requests_sent, statistic_num_requests_sent);
	emit(str_statistic_num_replies_sent, statistic_num_replies_sent);
	emit(str_statistic_num_beacons_sent, statistic_num_beacons_sent);
	emit(str_statistic_num_link_infos_sent, statistic_num_link_infos_sent);
	emit(str_statistic_num_cancelled_link_requests, statistic_num_cancelled_link_requests);
	emit(str_statistic_num_active_neighbors, statistic_num_active_neighbors);
	emit(str_statistic_min_beacon_offset, statistic_min_beacon_offset);
	emit(str_statistic_congestion, statistic_congestion);
	emit(str_statistic_contention, statistic_contention);
	emit(str_statistic_broadcast_candidate_slots, statistic_broadcast_candidate_slots);
}

const MCSOTDMA_Phy* MCSOTDMA_Mac::getPhy() const {
	return (MCSOTDMA_Phy*) lower_layer;
}

std::vector<std::pair<Reservation, const FrequencyChannel*>> MCSOTDMA_Mac::getReservations(unsigned int t) const {
	return reservation_manager->collectReservations(t);
}

void MCSOTDMA_Mac::setBroadcastTargetCollisionProb(double value) {
	((BCLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setTargetCollisionProb(value);
}
