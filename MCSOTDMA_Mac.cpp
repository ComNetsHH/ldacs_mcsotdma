//
// Created by Sebastian Lindner on 16.11.20.
//

#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "InetPacketPayload.hpp"
#include "PPLinkManager.hpp"
#include "SHLinkManager.hpp"
#include <IPhy.hpp>
#include <cassert>

using namespace TUHH_INTAIRNET_MCSOTDMA;

MCSOTDMA_Mac::MCSOTDMA_Mac(const MacId& id, uint32_t planning_horizon) : IMac(id), reservation_manager(new ReservationManager(planning_horizon)), active_neighbor_observer(50000) {
	stat_broadcast_mac_delay.dontEmitBeforeFirstReport();
	stat_min_beacon_offset.dontEmitBeforeFirstReport();
	stat_broadcast_candidate_slots.dontEmitBeforeFirstReport();
	stat_broadcast_selected_candidate_slots.dontEmitBeforeFirstReport();
	stat_pp_link_establishment_time.dontEmitBeforeFirstReport();	
}

MCSOTDMA_Mac::~MCSOTDMA_Mac() {
	for (auto& pair : link_managers)
		delete pair.second;
	delete reservation_manager;
}

void MCSOTDMA_Mac::notifyOutgoing(unsigned long num_bits, const MacId& mac_id) {
	coutd << *this << "::notifyOutgoing(bits=" << num_bits << ", id=" << mac_id << ")... ";	
	// tell the manager about new data
	if (mac_id != id)
		getLinkManager(mac_id)->notifyOutgoing(num_bits);
}

void MCSOTDMA_Mac::passToLower(L2Packet* packet, unsigned int center_frequency) {
	assert(lower_layer && "MCSOTDMA_Mac's lower layer is unset.");
	// check that the packet is not empty
	if (packet->getDestination() == SYMBOLIC_ID_UNSET) {
		delete packet;
		return;
	}
	statisticReportPacketSent();
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
	// Notify the third-party links.
	for (auto &item : third_party_links) 
		item.second.onSlotStart(num_slots);
	
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
				link_manager->onReceptionReservation(0);
				onReceptionSlot(channel);
				break;
			}			
			case Reservation::TX: {
				// Ensure that we have no simultaneous transmissions scheduled.
				num_txs++;
				if (num_txs > num_transmitters) {
					std::stringstream ss;
					ss << "MCSOTDMA_Mac::execute for too many transmissions within this time slot: ";					
					if (reservation_manager->getBroadcastReservationTable()->getReservation(0).isTx())
						ss << "SHTable: " << reservation_manager->getBroadcastReservationTable()->getReservation(0) << "; ";					
					for (const auto *tbl : reservation_manager->getP2PReservationTables()) {													
						if (tbl->getReservation(0).isTx())
							ss << "PPTable(" << *tbl->getLinkedChannel() << "): " << tbl->getReservation(0) << "; ";						
					}					
					if (reservation_manager->getTxTable()->getReservation(0).isTx())
						ss << "TXTable: " << reservation_manager->getTxTable()->getReservation(0) << "; ";					
					throw std::runtime_error(ss.str());
				}
				// Find the corresponding LinkManager.
				const MacId& id = reservation.getTarget();
				LinkManager* link_manager = getLinkManager(id);
				// Tell it about the transmission slot.				
				L2Packet* outgoing_packet = link_manager->onTransmissionReservation(0);
				if (outgoing_packet != nullptr) {
					outgoing_packet->notifyCallbacks();				
					passToLower(outgoing_packet, channel->getCenterFrequency());					
				} else {					
					coutd << "got empty packet from link manager; this is a wasted TX reservation -> ";
					if (id == SYMBOLIC_LINK_ID_BROADCAST || id == SYMBOLIC_LINK_ID_BEACON)
						this->stat_broadcast_wasted_tx_opportunities.increment();
					else
						this->stat_unicast_wasted_tx_opportunities.increment();
				}
				break;
			}			
			case Reservation::TX_BEACON: {
				num_txs++;
				if (num_txs > num_transmitters)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many transmissions within this time slot.");
				passToLower(getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onTransmissionReservation(0), channel->getCenterFrequency());
			}
			case Reservation::RX_BEACON: {
				num_rxs++;
				if (num_rxs > num_receivers)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many receptions within this time slot.");
				getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onReceptionReservation(0);
			}
		}
		coutd.decreaseIndent();
		coutd << std::endl;
	}
	coutd.decreaseIndent();
	return {num_txs, num_rxs};
}

void MCSOTDMA_Mac::receiveFromLower(L2Packet* packet, uint64_t center_frequency) {
	const MacId& origin_id = packet->getOrigin();
	const MacId& dest_id = packet->getDestination();
	coutd << *this << "::onPacketReception(from=" << origin_id << ", to=" << dest_id << ", f=" << center_frequency << "kHz)... ";
	if (origin_id == id) {
		this->deletePacket(packet);
		delete packet;
		return;
	}
	if (dest_id == SYMBOLIC_ID_UNSET)
		throw std::invalid_argument("MCSOTDMA_Mac::onPacketReception for unset dest_id.");	
	// store until slot end, then process
	received_packets[center_frequency].push_back(packet);
	coutd << "stored until slot end.";
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
			link_manager = new SHLinkManager(reservation_manager, this, 1);
			link_manager->assign(reservation_manager->getBroadcastFreqChannel());
		} else {						
			link_manager = new PPLinkManager(internal_id, reservation_manager, this);
			// ((PPLinkManager*) link_manager)->setForceBidirectionalLinks(this->should_force_bidirectional_links);			
		}		
		auto insertion_result = link_managers.insert(std::map<MacId, LinkManager*>::value_type(internal_id, link_manager));
		if (!insertion_result.second)
			throw std::runtime_error("Attempted to insert new LinkManager, but there already was one.");

	}
	return link_manager;
}

ThirdPartyLink& MCSOTDMA_Mac::getThirdPartyLink(const MacId& id1, const MacId& id2) {
	// look for an existing link
	auto it = third_party_links.find({id1, id2});
	if (it == third_party_links.end())
		it = third_party_links.find({id2, id1});	
	// if found, return it
	if (it != third_party_links.end()) 
		return (*it).second;
	// else, create one
	else {
		auto it_success = third_party_links.emplace(std::piecewise_construct, std::make_tuple(id1, id2), std::make_tuple(id1, id2, this));
		if (!it_success.second)
			throw std::runtime_error("couldn't emplace third-party link");		
		return it_success.first->second;
	}
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
		
		// single packets
		if (packets.size() == 1) {
			L2Packet *packet = packets.at(0);
			// might have a channel error
			if (packet->hasChannelError) {
				coutd << *this << " dropping packet due to channel error -> ";
				this->deletePacket(packet);
				delete packet;
				packets.erase(packets.begin());
				stat_num_channel_errors.increment();
			// otherwise they're received
			} else {
				coutd << *this << " processing packet -> ";
				if (packet->getDestination() == SYMBOLIC_LINK_ID_BROADCAST || packet->getDestination() == SYMBOLIC_LINK_ID_BEACON)
					getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onPacketReception(packet);
				else
					getLinkManager(packet->getOrigin())->onPacketReception(packet);				
				stat_num_packets_rcvd.increment();
			}			
		// several packets are cause for a collision
		} else if (packets.size() > 1) {			
			coutd << *this << " collision on frequency " << freq << " -> dropping " << packets.size() << " packets -> ";
			stat_num_packet_collisions.incrementBy(packets.size());

            for (auto *packet : packets) {
                this->deletePacket(packet);
                delete packet;
            }
        }
	}
	received_packets.clear();

	// update link managers
	for (auto item : link_managers)
		item.second->onSlotEnd();

	// update third-party links
	for (auto &item : third_party_links)
		item.second.onSlotEnd();

	// update active neighbors list
	active_neighbor_observer.onSlotEnd();
	statisticReportNumActiveNeighbors(active_neighbor_observer.getNumActiveNeighbors());

	// Statistics reporting.
	for (auto* stat : statistics)
		stat->update();
}

const MCSOTDMA_Phy* MCSOTDMA_Mac::getPhy() const {
	return (MCSOTDMA_Phy*) lower_layer;
}

std::vector<std::pair<Reservation, const FrequencyChannel*>> MCSOTDMA_Mac::getReservations(unsigned int t) const {
	return reservation_manager->collectReservations(t);
}

void MCSOTDMA_Mac::setBroadcastTargetCollisionProb(double value) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setTargetCollisionProb(value);
}

void MCSOTDMA_Mac::setBcSlotSelectionMinNumCandidateSlots(int value) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setMinNumCandidateSlots(value);
}

void MCSOTDMA_Mac::setBcSlotSelectionMaxNumCandidateSlots(int value) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setMaxNumCandidateSlots(value);
}

void MCSOTDMA_Mac::setContentionMethod(ContentionMethod method) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setUseContentionMethod(method);
}

void MCSOTDMA_Mac::setAlwaysScheduleNextBroadcastSlot(bool value) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setAlwaysScheduleNextBroadcastSlot(value);
}

void MCSOTDMA_Mac::reportNeighborActivity(const MacId& id) {
	active_neighbor_observer.reportActivity(id);
}

const NeighborObserver& MCSOTDMA_Mac::getNeighborObserver() const {
	return this->active_neighbor_observer;
}

void MCSOTDMA_Mac::setMinBeaconOffset(unsigned int value) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setMinBeaconInterval(value);
}

void MCSOTDMA_Mac::setMaxBeaconOffset(unsigned int value) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setMaxBeaconInterval(value);
}

void MCSOTDMA_Mac::setForceBidirectionalLinks(bool flag) {
	// this sets the flag which treats link managers that are created in the future
	IMac::setForceBidirectionalLinks(flag);	
	// now also handle those that already exist
	for (auto pair : link_managers) {
		if (pair.first != SYMBOLIC_LINK_ID_BEACON && pair.first != SYMBOLIC_LINK_ID_BROADCAST) {			
			((PPLinkManager*) pair.second)->setForceBidirectionalLinks(flag);			
		}
	}	
}

size_t MCSOTDMA_Mac::getNumUtilizedP2PResources() const {
	size_t n = 0;	
	for (const auto pair : link_managers) 
		if (pair.first != SYMBOLIC_LINK_ID_BEACON && pair.first != SYMBOLIC_LINK_ID_BROADCAST)
			n += ((PPLinkManager*) pair.second)->getNumUtilizedResources();					
	return n;
}

unsigned int MCSOTDMA_Mac::getP2PBurstOffset() const {
	return this->default_p2p_link_burst_offset;
}

void MCSOTDMA_Mac::setWriteResourceUtilizationIntoBeacon(bool flag) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setWriteResourceUtilizationIntoBeacon(flag);
}

void MCSOTDMA_Mac::setEnableBeacons(bool flag) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setEnableBeacons(flag);
}

void MCSOTDMA_Mac::setAdvertiseNextBroadcastSlotInCurrentHeader(bool flag) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setAdvertiseNextSlotInCurrentHeader(flag);
}