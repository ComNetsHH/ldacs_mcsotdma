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

#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "InetPacketPayload.hpp"
#include "PPLinkManager.hpp"
#include "SHLinkManager.hpp"
#include <IPhy.hpp>
#include <cassert>

using namespace TUHH_INTAIRNET_MCSOTDMA;

MCSOTDMA_Mac::MCSOTDMA_Mac(const MacId& id, uint32_t planning_horizon) : IMac(id), reservation_manager(new ReservationManager(planning_horizon)), neighbor_observer(50000), duty_cycle(DutyCycle(default_duty_cycle_period, default_max_duty_cycle, default_min_num_supported_pp_links)) {
	stat_broadcast_mac_delay.dontEmitBeforeFirstReport();	
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
	// stop transmission if this node is silent
	if (silent) {
		delete packet;
		return;
	}
	assert(lower_layer && "MCSOTDMA_Mac's lower layer is unset.");
	// check that the packet is not empty	
	if (packet->getDestination() == SYMBOLIC_ID_UNSET) {
		delete packet;
		return;
	}
	statisticReportPacketSent();
	lower_layer->receiveFromUpper(packet, center_frequency);
	num_sent_packets_this_slot++;
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
	
	if (learn_dme_activity) {		
		// reset channel sensing
		channel_sensing_observation.clear();
	}
}

std::pair<size_t, size_t> MCSOTDMA_Mac::execute() {
	// Fetch all reservations of the current time slot.
	std::vector<std::pair<Reservation, const FrequencyChannel*>> reservations = reservation_manager->collectCurrentReservations();	
	size_t num_txs = 0, num_rxs = 0;
	bool has_printed = false;
	for (const std::pair<Reservation, const FrequencyChannel*>& pair : reservations) {
		const Reservation& reservation = pair.first;
		const FrequencyChannel* channel = pair.second;

		if (reservation != Reservation()) {
			if (!has_printed) {
				coutd << *this << " processing " << reservations.size() << " reservations..." << std::endl;	
				has_printed = true;
			}
			coutd << *channel << ":" << reservation << std::endl;		
		}
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
				LinkManager *link_manager;
				// there might be a non-SH reception on the SH e.g. for link replies
				// these should not go to a PP link manager, who would falsely report a missing packet
				if (channel->isSH())
					link_manager = getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
				else				
					link_manager = getLinkManager(reservation.getTarget());				
				link_manager->onReceptionReservation();
				onReceptionSlot(channel);
				break;
			}			
			case Reservation::TX: {
				// Ensure that we have no simultaneous transmissions scheduled.
				num_txs++;
				if (num_txs > num_transmitters) {
					std::stringstream ss;
					ss << *this << "::execute(TX) error of too many transmissions (" << num_txs << "): ";										
					ss << "SHTable: " << reservation_manager->getBroadcastReservationTable()->getReservation(0) << "; ";					
					for (const auto *tbl : reservation_manager->getP2PReservationTables()) {													
						if (tbl->getReservation(0).isTx())
							ss << "PPTable(" << *tbl->getLinkedChannel() << "): " << tbl->getReservation(0) << "; ";						
					}										
					ss << "TXTable: " << reservation_manager->getTxTable()->getReservation(0) << "; ";					
					throw std::runtime_error(ss.str());
				}
				// Find the corresponding LinkManager.
				const MacId& id = reservation.getTarget();
				LinkManager* link_manager = getLinkManager(id);
				// Tell it about the transmission slot.				
				L2Packet* outgoing_packet = link_manager->onTransmissionReservation();
				if (outgoing_packet != nullptr) {
					outgoing_packet->notifyCallbacks();				
					coutd << "passing to lower layer -> ";
					passToLower(outgoing_packet, channel->getCenterFrequency());					
				} else {										
					coutd << "got empty packet from link manager; this is a wasted TX reservation -> ";					
				}
				break;
			}			
			case Reservation::TX_BEACON: {
				num_txs++;
				if (num_txs > num_transmitters) {
					std::stringstream ss;
					ss << *this << "::execute(TX_BEACON) error of too many transmissions: ";					
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
				passToLower(getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onTransmissionReservation(), channel->getCenterFrequency());
			}
			case Reservation::RX_BEACON: {
				num_rxs++;
				if (num_rxs > num_receivers)
					throw std::runtime_error("MCSOTDMA_Mac::execute for too many receptions within this time slot.");
				getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onReceptionReservation();
			}
		}				
	}	
	// keep track of the number of transmissions w.r.t. the duty cycle
	duty_cycle.reportNumTransmissions(num_txs);
	// emit the duty cycle once enough values have been recorded
	if (duty_cycle.shouldEmitStatistic()) {
		stat_duty_cycle.capture(duty_cycle.get());
		stat_duty_cycle.update();
	}

	return {num_txs, num_rxs};
}

void MCSOTDMA_Mac::storePacket(L2Packet *&packet, uint64_t center_freq) {	
	received_packets[center_freq].push_back(packet);
	coutd << "stored until slot end -> ";
}

void MCSOTDMA_Mac::receiveFromLower(L2Packet* packet, uint64_t center_frequency) {
	const MacId& origin_id = packet->getOrigin();
	const MacId& dest_id = packet->getDestination();
	coutd << *this << "::onPacketReception(from=" << origin_id << ", to=" << dest_id << ", f=" << center_frequency << "kHz)... ";
	if (origin_id == id) {
		this->deletePacket(packet);
		delete packet;
		coutd << "deleted -> ";		
		return;
	}
	if (dest_id == SYMBOLIC_ID_UNSET && origin_id != SYMBOLIC_LINK_ID_DME)
		throw std::invalid_argument("MCSOTDMA_Mac::onPacketReception for unset dest_id.");	
	// store until slot end, then process	
	storePacket(packet, center_frequency);		
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
			((PPLinkManager*) link_manager)->setMaxNoPPLinkEstablishmentAttempts(this->max_no_pp_link_establishment_attempts);
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

void MCSOTDMA_Mac::notifyAboutDmeTransmission(uint64_t center_frequency) {
	if (learn_dme_activity)
		channel_sensing_observation[center_frequency] = true;
}

void MCSOTDMA_Mac::onSlotEnd() {
	size_t num_dropped_packets_this_slot = 0;
	size_t num_rcvd_packets_this_slot = 0;

	for (auto &packet_freq_pair : received_packets) {
		// On this frequency channel,
		uint64_t freq = packet_freq_pair.first;
		// these packets were received.
		std::vector<L2Packet*> packets = packet_freq_pair.second;		
		// remove DME packets before processing
		for (auto it = packets.begin(); it != packets.end();) {
			auto *packet = *it;
			if (packet->isDME()) {							
				// remember on which channel 
				if (learn_dme_activity) {
					channel_sensing_observation[freq] = true;					
				}
				this->deletePacket(packet);
				delete packet;
				it = packets.erase(it);
				stat_num_dme_packets_rcvd.increment();
			} else
				it++;
		} 
		
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
				try {
					reportNeighborActivity(packet->getOrigin());
					if (packet->getDestination() == SYMBOLIC_LINK_ID_BROADCAST || packet->getDestination() == SYMBOLIC_LINK_ID_BEACON)
						getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onPacketReception(packet);
					else
						getLinkManager(packet->getOrigin())->onPacketReception(packet);				
					stat_num_packets_rcvd.increment();
					num_rcvd_packets_this_slot = 1;
				} catch (const std::exception &e) {
					std::stringstream ss;
					ss << *this << "::onSlotEnd error processing received packet: " << e.what() << std::endl;						
					throw std::runtime_error(ss.str());
				}				
			}			
		// several packets are cause for a collision
		} else if (packets.size() > 1) {			
			coutd << *this << " collision on frequency " << freq << " -> dropping " << packets.size() - 1 << " packets -> ";
			stat_num_packet_collisions.increment();
			// figure out which packet to keep
			// by comparing SINRs
			L2Packet *packet_with_largest_snr = nullptr;
			double largest_snr = 0.0;
			for (auto *packet : packets) {
				if (packet->snr > largest_snr) {
					largest_snr = packet->snr;
					packet_with_largest_snr = packet;
				}
			}

			if (packet_with_largest_snr != nullptr) {
				coutd << *this << " processing packet with largest SNR -> ";				
				try {
					reportNeighborActivity(packet_with_largest_snr->getOrigin());
					if (packet_with_largest_snr->getDestination() == SYMBOLIC_LINK_ID_BROADCAST || packet_with_largest_snr->getDestination() == SYMBOLIC_LINK_ID_BEACON)
						getLinkManager(SYMBOLIC_LINK_ID_BROADCAST)->onPacketReception(packet_with_largest_snr);
					else
						getLinkManager(packet_with_largest_snr->getOrigin())->onPacketReception(packet_with_largest_snr);				
					stat_num_packets_rcvd.increment();
					num_rcvd_packets_this_slot = 1;
				} catch (const std::exception &e) {
					std::stringstream ss;
					ss << *this << "::onSlotEnd error processing received packet: " << e.what() << std::endl;						
					throw std::runtime_error(ss.str());
				}				
			}

            for (auto *packet : packets) {			
				if (packet != packet_with_largest_snr) {
					this->deletePacket(packet);
					delete packet;
					num_dropped_packets_this_slot++;
				}
            }
        }
	}
	received_packets.clear();

	// update link managers
	try {
		for (auto item : link_managers)
			item.second->onSlotEnd();
	} catch (const std::exception &e) {
		std::stringstream ss;
		ss << *this << "::onSlotEnd error updating link managers: " << e.what() << std::endl;		
		throw std::runtime_error(ss.str());
	}

	// update third-party links
	try {
		for (auto &item : third_party_links)
			item.second.onSlotEnd();
	} catch (const std::exception &e) {
		std::stringstream ss;
		ss << *this << "::onSlotEnd error updating third party links: " << e.what() << std::endl;		
		throw std::runtime_error(ss.str());
	}

	// update active neighbors list
	neighbor_observer.onSlotEnd();
	statisticReportNumActiveNeighbors(neighbor_observer.getNumActiveNeighbors());
	if (stat_num_broadcasts_rcvd.get() > 2.0) {
		double avg_beacon_delay = neighbor_observer.getAvgBeaconDelay();
		if (avg_beacon_delay > 0.0)
			statisticReportAvgBeaconReceptionDelay(avg_beacon_delay);
		double avg_first_neighbor_delay = neighbor_observer.getAvgFirstNeighborBeaconDelay();
		if (avg_first_neighbor_delay > 0.0)
			statisticReportFirstNeighborAvgBeaconReceptionDelay(avg_first_neighbor_delay);
	}

	// update per-slot statistics
	if (this->capture_per_slot_statistics) {
		stat_dropped_packets_this_slot.capture(num_dropped_packets_this_slot);
		stat_rcvd_packets_this_slot.capture(num_rcvd_packets_this_slot);
		stat_sent_packets_this_slot.capture(num_sent_packets_this_slot);
		num_sent_packets_this_slot = 0;		
	} 

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

void MCSOTDMA_Mac::reportNeighborActivity(const MacId& id) {
	neighbor_observer.reportActivity(id);
}

void MCSOTDMA_Mac::reportBroadcastSlotAdvertisement(const MacId& id, unsigned int advertised_slot_offset) {
	neighbor_observer.reportBroadcastSlotAdvertisement(id, advertised_slot_offset);
}



NeighborObserver& MCSOTDMA_Mac::getNeighborObserver() {
	return this->neighbor_observer;
}

size_t MCSOTDMA_Mac::getNumUtilizedP2PResources() const {
	size_t n = 0;	
	// for (const auto pair : link_managers) 
	// 	if (pair.first != SYMBOLIC_LINK_ID_BEACON && pair.first != SYMBOLIC_LINK_ID_BROADCAST)
	// 		n += ((PPLinkManager*) pair.second)->getNumUtilizedResources();					
	return n;
}

unsigned int MCSOTDMA_Mac::getP2PBurstOffset() const {
	unsigned int max_burst_offset = 0;
	// if (link_managers.size() <= 1) 
	// 	max_burst_offset = default_p2p_link_burst_offset;
	// else {
	// 	for (auto pair : link_managers) {
	// 		MacId id = pair.first;
	// 		if (id != SYMBOLIC_LINK_ID_BROADCAST && id != SYMBOLIC_LINK_ID_BEACON) {
	// 			auto *link_manager = (PPLinkManager*) pair.second;
	// 			if (link_manager->getBurstOffset() > max_burst_offset)
	// 				max_burst_offset = link_manager->getBurstOffset();
	// 		}
	// 	}
	// }
	return max_burst_offset;
}

void MCSOTDMA_Mac::setAdvertiseNextBroadcastSlotInCurrentHeader(bool flag) {
	((SHLinkManager*) getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->setAdvertiseNextSlotInCurrentHeader(flag);
}

void MCSOTDMA_Mac::onThirdPartyLinkReset(const ThirdPartyLink* caller) {
	for (auto &pair : third_party_links) {
		auto &third_party_link = pair.second;
		if (third_party_link != *caller) {
			third_party_link.onAnotherThirdLinkReset();
		}
	}
}

bool MCSOTDMA_Mac::isGoingToTransmitDuringCurrentSlot(uint64_t center_frequency) const {
	std::vector<std::pair<Reservation, const FrequencyChannel*>> reservations = reservation_manager->collectCurrentReservations();
	for (const auto &pair : reservations) {
		uint64_t freq = pair.second->getCenterFrequency();
		if (freq == center_frequency) {
			if (pair.first.isAnyTx())
				return true;
		}
	}
	return false;
}

const std::vector<int> MCSOTDMA_Mac::getChannelSensingObservation() const {
	if (!this->learn_dme_activity)
		throw std::runtime_error("getChannelSensingObservation cannot be called when learn_dme_activity is false!");
	auto observation = std::vector<int>();	
	std::vector<FrequencyChannel*> channels = std::vector<FrequencyChannel*>(reservation_manager->getP2PFreqChannels());	
	std::sort(channels.begin(), channels.end(), [](const FrequencyChannel *chn1, const FrequencyChannel *chn2) -> bool { return chn1->getCenterFrequency() < chn2->getCenterFrequency();});	

	for (const auto *channel : channels) {
		bool sensed_signal = false;
		try {
			const bool &ref = this->channel_sensing_observation.at(channel->getCenterFrequency());
			sensed_signal = ref;
		} catch (const std::exception &e) {
			sensed_signal = false;
		}
		if (sensed_signal == true)
			observation.push_back(-1);
		else
			observation.push_back(1);
	}	
	return observation;
}

void MCSOTDMA_Mac::setDutyCycle(unsigned int period, double max, unsigned int min_num_supported_pp_links) {
	this->duty_cycle = DutyCycle(period, max, min_num_supported_pp_links);
}

void MCSOTDMA_Mac::setLearnDMEActivity(bool value) {
	this->learn_dme_activity = value;
}

void MCSOTDMA_Mac::passPrediction(const std::vector<std::vector<double>>& prediction_mat) {
	this->current_prediction_mat = prediction_mat;
}

std::vector<std::vector<double>>& MCSOTDMA_Mac::getCurrentPrediction() {
	return this->current_prediction_mat;
}

bool MCSOTDMA_Mac::shouldLearnDmeActivity() const {
	return this->learn_dme_activity;
}

std::pair<std::vector<double>, std::vector<int>> MCSOTDMA_Mac::getUsedPPDutyCycleBudget() const {
	std::pair<std::vector<double>, std::vector<int>> contributions;	
	std::vector<double> &used_pp_duty_cycle_budget = contributions.first;
	std::vector<int> &timeouts = contributions.second;
	for (const auto &pair : link_managers) {
		const MacId &id = pair.first;
		const auto *link_manager = pair.second;		
		if (id != SYMBOLIC_LINK_ID_BROADCAST && id != SYMBOLIC_LINK_ID_BEACON && id != SYMBOLIC_LINK_ID_DME) {			
			const auto *pp = (PPLinkManager*) link_manager;
			if (pp->isActive()) {				
				used_pp_duty_cycle_budget.push_back(link_manager->getNumTxPerTimeSlot());
				timeouts.push_back(pp->getRemainingTimeout());
			}
		}		
	}
	return contributions;
}

size_t MCSOTDMA_Mac::getNumActivePPLinks() const {
	size_t num_active_pps = 0;
	for (const auto &pair : link_managers) {
		const MacId &id = pair.first;
		const auto *link_manager = pair.second;		
		if (id != SYMBOLIC_LINK_ID_BROADCAST && id != SYMBOLIC_LINK_ID_BEACON && id != SYMBOLIC_LINK_ID_DME) {			
			const auto *pp = (PPLinkManager*) link_manager;
			if (pp->isActive()) {				
				num_active_pps++;
			}
		}
	}
	return num_active_pps;
}

double MCSOTDMA_Mac::getUsedSHDutyCycleBudget() const {
	return ((SHLinkManager*) link_managers.at(SYMBOLIC_LINK_ID_BROADCAST))->getNumTxPerTimeSlot();
}

int MCSOTDMA_Mac::getSHSlotOffset() const {
	return ((SHLinkManager*) link_managers.at(SYMBOLIC_LINK_ID_BROADCAST))->getNextBroadcastSlot();
}

const DutyCycle& MCSOTDMA_Mac::getDutyCycle() const {
	return this->duty_cycle;
}


int MCSOTDMA_Mac::getDefaultPPLinkTimeout() const {
	return this->default_pp_link_timeout;
}

const std::map<MacId, LinkManager*>& MCSOTDMA_Mac::getLinkManagers() const {
	return this->link_managers;
}

std::vector<L2HeaderSH::LinkUtilizationMessage> MCSOTDMA_Mac::getPPLinkUtilizations() const {
	auto utilizations = std::vector<L2HeaderSH::LinkUtilizationMessage>();
	for (const auto pair : link_managers) {
		const MacId &id = pair.first;
		if (id != SYMBOLIC_LINK_ID_BROADCAST && id != SYMBOLIC_LINK_ID_BEACON) {
			const PPLinkManager *link_manager = (PPLinkManager*) pair.second;
			if (link_manager->isActive()) {
				L2HeaderSH::LinkUtilizationMessage utilization = link_manager->getUtilization();
				if (utilization != L2HeaderSH::LinkUtilizationMessage())
					utilizations.push_back(utilization);
			}
		}
	}
	return utilizations;
}

void MCSOTDMA_Mac::setMaxNoPPLinkEstablishmentAttempts(int value) {
	this->max_no_pp_link_establishment_attempts = value;
	for (auto pair : link_managers) {
		if (pair.first != SYMBOLIC_LINK_ID_BROADCAST && pair.first != SYMBOLIC_LINK_ID_BEACON) {
			auto *pp = (PPLinkManager*) pair.second;
			pp->setMaxNoPPLinkEstablishmentAttempts(this->max_no_pp_link_establishment_attempts);
		}
	}
}

void MCSOTDMA_Mac::setConsiderDutyCycle(bool flag) {
	this->use_duty_cycle = flag;
}

bool MCSOTDMA_Mac::shouldConsiderDutyCycle() const {
	return this->use_duty_cycle;
}

void MCSOTDMA_Mac::setMinNumSupportedPPLinks(unsigned int value) {
	this->duty_cycle.setMinNumSupportedPPLinks(value);
}

void MCSOTDMA_Mac::setForcePPPeriod(bool flag, int value) {
	this->should_force_pp_period = flag;
	this->forced_pp_period = value;
}

bool MCSOTDMA_Mac::shouldUseFixedPPPeriod() const {
	return this->should_force_pp_period;
}

int MCSOTDMA_Mac::getFixedPPPeriod() const {
	return this->forced_pp_period;
}


void MCSOTDMA_Mac::setDutyCycleBudgetComputationStrategy(const DutyCycleBudgetStrategy& strategy) {
	this->duty_cycle.setStrategy(strategy);
}

void MCSOTDMA_Mac::setSilent(bool is_silent) {
	this->silent = is_silent;
}