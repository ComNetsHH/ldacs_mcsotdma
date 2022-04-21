//
// Created by seba on 2/18/21.
//

#include <sstream>
#include "SHLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "PPLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

SHLinkManager::SHLinkManager(ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int min_beacon_gap)
: LinkManager(SYMBOLIC_LINK_ID_BROADCAST, reservation_manager, mac), avg_num_slots_inbetween_packet_generations(100), beacon_module() {
	beacon_module.setMinBeaconGap(min_beacon_gap);
}

void SHLinkManager::onReceptionReservation() {

}

L2Packet* SHLinkManager::onTransmissionReservation() {
	coutd << *mac << "::" << *this << "::onTransmissionReservation -> ";	

	auto *packet = new L2Packet();
	auto *base_header = new L2HeaderBase(mac->getMacId(), 0, 1, 1, 0);
	packet->addMessage(base_header, nullptr);
	unsigned long capacity = mac->getCurrentDatarate();

	bool is_beacon = false;

	// Beacon slots are exclusive to beacons.
	if (beacon_module.isEnabled() && beacon_module.shouldSendBeaconThisSlot()) {
		coutd << "broadcasting beacon -> ";
		// Schedule next beacon slot.
		scheduleBeacon(); // prints when the next beacon slot is scheduled
		coutd << "while non-beacon broadcast is " << (next_broadcast_scheduled ? "scheduled in " + std::to_string(next_broadcast_slot) + " slots -> " : "not scheduled -> ");

		// Generate beacon message.
		auto hostPosition = this->mac->getHostPosition();
		packet->addMessage(beacon_module.generateBeacon(reservation_manager->getP2PReservationTables(), reservation_manager->getBroadcastReservationTable(), hostPosition, mac->getNumUtilizedP2PResources(), mac->getP2PBurstOffset()));
		mac->statisticReportBeaconSent();
		is_beacon = true;
		base_header->burst_offset = beacon_module.getNextBeaconSlot();
	// Non-beacon slots can be used for any other type of broadcast.
	} else {
		coutd << "broadcasting data -> ";		
		// prioritize link replies
		std::vector<std::pair<L2HeaderLinkReply*, LinkManager::LinkEstablishmentPayload*>> replies_to_add;
		for (auto it = link_replies.begin(); it != link_replies.end(); it++) {
			auto pair = *it;
			coutd << "checking link reply in " << pair.first << " slots...";
			if (pair.first == 0) {				
				auto &header_payload_pair = pair.second;
				if (header_payload_pair.first->getBits() + header_payload_pair.second->getBits() <= capacity) {
					replies_to_add.push_back(pair.second);
					link_replies.erase(it);
					it--;
					capacity -= pair.second.first->getBits() + pair.second.second->getBits();
					coutd << "added link reply for '" << pair.second.first->dest_id << "' to broadcast -> ";
					const L2HeaderLinkReply *header = pair.second.first;					
					mac->statisticReportLinkReplySent();
				}
			}
		}
		// prioritize link requests
		std::vector<std::pair<L2HeaderLinkRequest*, LinkManager::LinkEstablishmentPayload*>> requests_to_add;		
		while (!link_requests.empty()) {
			// Fetch next link request.
			auto &pair = link_requests.at(0);
			// Compute payload.
			if (pair.second->callback == nullptr)
				throw std::invalid_argument("SHLinkManager::onTransmissionReservation has nullptr link request callback - can't populate the LinkRequest!");
			try {
				pair.second->callback->populateLinkRequest(pair.first, pair.second);
			} catch (const not_viable_error &e) {
				coutd << "ignoring invalid link request -> ";				
				delete (*link_requests.begin()).first;
				delete (*link_requests.begin()).second;
				link_requests.erase(link_requests.begin());
				continue;
			} catch (const std::exception &e) {				
				std::cerr << "error while populating link request" << e.what() << std::endl;				
				throw e;
			}
			// Add to the packet if it fits.
			if (pair.first->getBits() + pair.second->getBits() <= capacity) {
				requests_to_add.push_back(pair);				
				link_requests.erase(link_requests.begin());
				capacity -= pair.first->getBits() + pair.second->getBits();
				coutd << "added link request for '" << pair.first->dest_id << "' to broadcast -> ";
				mac->statisticReportLinkRequestSent();
			} else
				break; // Stop if it doesn't fit anymore.
		}
		// Add broadcast data.
		int remaining_bits = capacity - packet->getBits() + base_header->getBits(); // The requested packet will have a base header, which we'll drop, so add it to the requested number of bits.
		if (remaining_bits > 0) {
			coutd << "requesting " << remaining_bits << " bits from upper sublayer -> ";
			L2Packet* upper_layer_data = mac->requestSegment(remaining_bits, link_id);
			size_t num_bits_added = 0;
			for (size_t i = 0; i < upper_layer_data->getPayloads().size(); i++) {
				const auto &upper_layer_header = upper_layer_data->getHeaders().at(i);
				const auto &upper_layer_payload = upper_layer_data->getPayloads().at(i);
				// ignore empty broadcasts
				if (upper_layer_header->frame_type == L2Header::broadcast) {
					if (upper_layer_payload == nullptr || (upper_layer_payload != nullptr && upper_layer_payload->getBits() == 0)) {						
						coutd << "ignoring empty broadcast -> ";
						continue;
					}
				}				
				if (upper_layer_header->frame_type != L2Header::base) {					
					// copy
					L2Header *header = upper_layer_header->copy();
					L2Packet::Payload *payload = upper_layer_data->getPayloads().at(i)->copy();
					// add
					packet->addMessage(header, payload);
					num_bits_added += header->getBits() + payload->getBits();
					coutd << "added '" << header->frame_type << "' message -> ";					
				}
			}
			coutd << "added " << num_bits_added << " bits -> ";
//			if (num_bits_added > remaining_bits) {
//				std::stringstream ss;
//				ss << *mac << "::" << *this << "::onTransmissionReservation error: " << num_bits_added << " bits were returned by upper sublayer instead of requested " << remaining_bits << "!";
//				throw std::runtime_error(ss.str());
//			}
			delete upper_layer_data;
		}
		
		// if no data has been added so far, but link requests or replies should be added
		// add a broadcast header and no payload first
		if (packet->getHeaders().size() == 1 && (!requests_to_add.empty() || !replies_to_add.empty())) 
			packet->addMessage(new L2HeaderBroadcast(), nullptr);
		for (const auto &pair : replies_to_add)
			packet->addMessage(pair.first, pair.second);		
		for (const auto &pair : requests_to_add)
			packet->addMessage(pair.first, pair.second);		

		bool should_schedule_next_broadcast_slot = always_schedule_next_slot || !link_requests.empty() || mac->isThereMoreData(link_id);
		if (should_schedule_next_broadcast_slot) {
			coutd << "scheduling next broadcast slot -> ";
			try {
				scheduleBroadcastSlot();
				coutd << "next broadcast in " << next_broadcast_slot << " slots -> ";
				// Put it into the header.
				if (this->advertise_slot_in_header) {
					coutd << "advertising slot in header -> ";
					base_header->burst_offset = next_broadcast_slot;
				}
			} catch (const std::exception &e) {
				throw std::runtime_error("Error when trying to schedule next broadcast because there's more data: " + std::string(e.what()));
			}
		} else {
			next_broadcast_scheduled = false;
			next_broadcast_slot = 0;
			coutd << "no more broadcast data, not scheduling a next slot -> ";
		}		
	}

	if (packet->getHeaders().size() == 1) {		
		delete packet;
		return nullptr;
	} else {
		if (!is_beacon) {
			mac->statisticReportBroadcastSent();
			mac->statisticReportBroadcastMacDelay(measureMacDelay());		
		}
		return packet;
	}
}

void SHLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	packet_generated_this_slot = true;
	if (!next_broadcast_scheduled) {
		coutd << "scheduling next broadcast -> ";
		try {
			scheduleBroadcastSlot();
			coutd << "next broadcast in " << next_broadcast_slot << " slots -> ";
		} catch (const std::exception &e) {
			throw std::runtime_error("Error when trying to schedule broadcast because of new data: " + std::string(e.what()));
		}
	}	
	// to account for application-layer starting times later than immediately,
	// normalize the MAC delay measurement to the first time this function is called (instead of zero)
	if (time_slot_of_last_channel_access == 0)
		time_slot_of_last_channel_access = mac->getCurrentSlot();
}

void SHLinkManager::onSlotStart(uint64_t num_slots) {
	// decrement next broadcast slot counter
	if (next_broadcast_scheduled) {
		if (next_broadcast_slot == 0)
			throw std::runtime_error("SHLinkManager(" + std::to_string(mac->getMacId().getId()) + ")::onSlotEnd would underflow next_broadcast_slot (was this transmission missed?)");
		next_broadcast_slot -= 1;
	} else
		next_broadcast_slot = 0;

	if (next_broadcast_scheduled || next_broadcast_scheduled)
		coutd << *mac << "::" << *this << "::onSlotStart(" << num_slots << ") -> ";
	if (next_broadcast_scheduled) {
		coutd << "next broadcast " << (next_broadcast_slot == 0 ? "now" : "in " + std::to_string(next_broadcast_slot) + " slots") << " -> ";
		if (reservation_manager->getTxTable()->getReservation(next_broadcast_slot).getAction() != Reservation::TX || reservation_manager->getBroadcastReservationTable()->getReservation(next_broadcast_slot).getAction() != Reservation::TX) {
			std::stringstream ss;
			ss << *mac << "::" << *this << "::onSlotStart for scheduled broadcast but invalid table: broadcast_in=" << next_broadcast_slot << " tx_table=" << reservation_manager->getTxTable()->getReservation(next_broadcast_slot) << " sh_table=" << reservation_manager->getBroadcastReservationTable()->getReservation(next_broadcast_slot) << "!";			
			throw std::runtime_error(ss.str());
		}			
	} else
		coutd << "no next broadcast scheduled -> ";
	if (next_beacon_scheduled) {
		coutd << "next beacon " << (getNextBeaconSlot() == 0 ? "now" : "in " + std::to_string(getNextBeaconSlot()) + " slots") << " -> ";
		if (reservation_manager->getTxTable()->getReservation(getNextBeaconSlot()).getAction() != Reservation::TX_BEACON || reservation_manager->getBroadcastReservationTable()->getReservation(getNextBeaconSlot()).getAction() != Reservation::TX_BEACON) {
			std::stringstream ss;
			ss << *mac << "::" << *this << "::onSlotStart for scheduled beacon but invalid table: beacon_in=" << getNextBeaconSlot() << " tx_table=" << reservation_manager->getTxTable()->getReservation(getNextBeaconSlot()) << " sh_table=" << reservation_manager->getBroadcastReservationTable()->getReservation(getNextBeaconSlot()) << "!";			
			throw std::runtime_error(ss.str());
		}			
	} else
		coutd << "no next beacon scheduled -> ";


	// broadcast link manager should always have a ReservationTable assigned
	if (current_reservation_table == nullptr)
		throw std::runtime_error("SHLinkManager::broadcastSlotSelection for unset ReservationTable.");

	// mark reception slot if there's nothing else to do
	const auto& current_reservation = current_reservation_table->getReservation(0);
	if (current_reservation.isIdle() || current_reservation.isBusy()) {
		coutd << "marking BC reception -> ";
		try {
			current_reservation_table->mark(0, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::RX));
		} catch (const std::exception& e) {
			throw std::runtime_error("SHLinkManager::onSlotStart(" + std::to_string(num_slots) + ") error trying to mark BC reception slot: " + e.what());
		}
	}
}

void SHLinkManager::onSlotEnd() {
	if (packet_generated_this_slot) {
		packet_generated_this_slot = false;
		avg_num_slots_inbetween_packet_generations.put(num_slots_since_last_packet_generation + 1);
		num_slots_since_last_packet_generation = 0;
	} else
		num_slots_since_last_packet_generation++;

	if (beacon_module.shouldSendBeaconThisSlot() || !next_beacon_scheduled) {
		// Schedule next beacon slot.
		scheduleBeacon();
	}

	// update counters that keep track when link replies are due
	for (auto &item : link_replies) {
		coutd << *mac << "::" << *this << " decrementing counter until link reply -> "; 
		if (item.first > 0) {
			coutd << item.first << "->";
			item.first--;			
			coutd << item.first << " -> ";
		} else {			
			coutd << "missed transmitting this reply -> ";
			// link reply couldn't have been sent, probably because a third-party link has unscheduled the transmission
			// erase it 
			const MacId dest_id = item.second.first->dest_id;			 
			cancelLinkReply(dest_id);
			// notify the corresponding link manager
			((PPLinkManager*) mac->getLinkManager(dest_id))->scheduledLinkReplyCouldNotHaveBeenSent();			
		}		
	}
	
	beacon_module.onSlotEnd();		
	LinkManager::onSlotEnd();
}

void SHLinkManager::sendLinkRequest(L2HeaderLinkRequest*& header, LinkManager::LinkEstablishmentPayload*& payload) {
	coutd << *this << " saving link request for transmission -> ";
	// save request
	link_requests.push_back({header, payload});	
	// schedule broadcast slot if necessary
	notifyOutgoing(header->getBits() + payload->getBits());
}

bool SHLinkManager::canSendLinkReply(unsigned int time_slot_offset) const {
	const Reservation &res = current_reservation_table->getReservation(time_slot_offset);
	return (res.isIdle() && mac->isTransmitterIdle(time_slot_offset, 1)) || res.isTx();
}

void SHLinkManager::sendLinkReply(L2HeaderLinkReply*& header, LinkEstablishmentPayload*& payload, unsigned int time_slot_offset) {
	coutd << *this << " saving link reply for transmission in " << time_slot_offset << " slots -> ";
	if (!canSendLinkReply(time_slot_offset))
		throw std::invalid_argument("SHLinkManager::sendLinkReply for a time slot offset where a reply cannot be sent.");
	// schedule reply
	if (current_reservation_table->getReservation(time_slot_offset).isIdle()) {
		current_reservation_table->mark(time_slot_offset, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX));		
		coutd << "reserved t=" << time_slot_offset <<": " << current_reservation_table->getReservation(time_slot_offset) << " -> ";				
		if (!next_broadcast_scheduled || next_broadcast_slot > time_slot_offset) {
			next_broadcast_scheduled = true;
			next_broadcast_slot = time_slot_offset;
		}
	} else {
		coutd << "already reserved (" << current_reservation_table->getReservation(time_slot_offset) << " ) -> ";
	}
	// save reply
	link_replies.push_back({time_slot_offset, {header, payload}});		
	coutd << "saved -> ";
}

size_t SHLinkManager::cancelLinkRequest(const MacId& id) {
	size_t num_removed = 0;
	for (auto it = link_requests.begin(); it != link_requests.end();) {
		const auto* header = it->first;
		if (header->getDestId() == id) {
			delete (*it).first;
			delete (*it).second;
			it = link_requests.erase(it);
			num_removed++;
		} else
			it++;
	}
	return num_removed;
}

size_t SHLinkManager::cancelLinkReply(const MacId& id) {
	size_t num_removed = 0;	
	for (auto it = link_replies.begin(); it != link_replies.end();) {
		const auto &pair = *it;
		const auto &reply_msg = pair.second;
		const auto *header = reply_msg.first;		
		if (header->dest_id == id) {
			delete reply_msg.first;
			delete reply_msg.second;
			it = link_replies.erase(it);			
			num_removed++;			
		} else
			it++;
	}
	return num_removed;
}

unsigned int SHLinkManager::getNumCandidateSlots(double target_collision_prob, unsigned int min, unsigned int max) const {
	if (target_collision_prob <= 0.0 || target_collision_prob >= 1.0)
		throw std::invalid_argument("SHLinkManager::getNumCandidateSlots target collision probability not between 0 and 1.");
	unsigned int k;		
	if (contention_method == ContentionMethod::binomial_estimate) {
		throw std::invalid_argument("binomial_estimate method no longer supported");	
	} else if (contention_method == ContentionMethod::poisson_binomial_estimate) {
		throw std::invalid_argument("poisson_binomial_estimate method no longer supported");
	// Assume that every neighbor that has been active within the contention window will again be active.
	} else if (contention_method == ContentionMethod::randomized_slotted_aloha) {
		// Number of active neighbors.
		double m = (double) mac->getNeighborObserver().getNumActiveNeighbors();
		k = std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / m)));
		coutd << "channel access method: randomized slotted ALOHA for " << m << " active neighbors -> ";
	// Don't make use of contention estimation in any way. Just select something out of the next seven idle slots.
	} else if (contention_method == ContentionMethod::naive_random_access) {
		k = 7;
		coutd << "channel access method: naive random access -> ";
	} else {
		throw std::invalid_argument("SHLinkManager::getNumCandidateSlots for unknown contention method: '" + std::to_string(contention_method) + "'.");
	}
	unsigned int final_candidates = std::min(max, std::max(min, k));
	coutd << "num_candidates=" << final_candidates << " -> ";
	return final_candidates;
}

unsigned long long SHLinkManager::nchoosek(unsigned long n, unsigned long k) const {
	if (k == 0)
		return 1;
	return (n * nchoosek(n - 1, k - 1)) / k;
}

unsigned int SHLinkManager::broadcastSlotSelection(unsigned int min_offset) {
	coutd << "broadcast slot selection -> ";
	if (current_reservation_table == nullptr)
		throw std::runtime_error("SHLinkManager::broadcastSlotSelection for unset ReservationTable.");
	unsigned int num_candidates = getNumCandidateSlots(this->broadcast_target_collision_prob, this->MIN_CANDIDATES, this->MAX_CANDIDATES);
	mac->statisticReportBroadcastCandidateSlots((size_t) num_candidates);
	coutd << "min_offset=" << (int) min_offset << " -> ";
	std::vector<unsigned int > candidate_slots = current_reservation_table->findSHCandidates(num_candidates, (int) min_offset);
	coutd << "found " << candidate_slots.size() << " -> ";
	if (candidate_slots.empty()) {
		coutd << "printing reservations over entire planning horizon: " << std::endl << "t\tlocal\t\tTX" << std::endl;
		for (size_t t = 0; t < current_reservation_table->getPlanningHorizon(); t++) 
			coutd << "t=" << t << ":\t" << current_reservation_table->getReservation(t) << "\t" << reservation_manager->getTxTable()->getReservation(t) << std::endl;
		throw std::runtime_error("SHLinkManager::broadcastSlotSelection found zero candidate slots.");
	}
	unsigned int selected_slot;
	try {
		selected_slot = candidate_slots.at(getRandomInt(0, candidate_slots.size()));	
	} catch (const std::exception &e) {
		std::cerr << "error during broadcast slot selection when trying to get a random integer -> is the 'num-rngs' parameter in the .ini too small?" << std::endl;
		throw std::runtime_error("error during broadcast slot selection when trying to get a random integer -> is the 'num-rngs' parameter in the .ini too small?");
	}
	mac->statisticReportSelectedBroadcastCandidateSlots(selected_slot);
	return selected_slot;
}

void SHLinkManager::scheduleBroadcastSlot() {	
	unscheduleBroadcastSlot();
	// By default, even the next slot could be chosen.
	unsigned int min_offset = 1;	
	// Apply slot selection.
	next_broadcast_slot = broadcastSlotSelection(min_offset);
	next_broadcast_scheduled = true;
	current_reservation_table->mark(next_broadcast_slot, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX));			
}

void SHLinkManager::unscheduleBroadcastSlot() {
	if (next_broadcast_scheduled) {
		current_reservation_table->mark(next_broadcast_slot, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
		next_broadcast_slot = 0;
		next_broadcast_scheduled = false;
	}
}

void SHLinkManager::processBeaconMessage(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	coutd << "parsing incoming beacon -> ";
	auto pair = beacon_module.parseBeacon(origin_id, (const BeaconPayload*&) payload, reservation_manager);
	if (pair.first) {
		beaconCollisionDetected(origin_id, Reservation::RX);		
	} if (pair.second) {
		broadcastCollisionDetected(origin_id, Reservation::RX);
	}
	// pass it to the MAC layer
	mac->onBeaconReception(origin_id, L2HeaderBeacon(*header));
}

bool SHLinkManager::isNextBroadcastScheduled() const {
	return next_broadcast_scheduled;
}

unsigned int SHLinkManager::getNextBroadcastSlot() const {
	int earliest_transmission = next_broadcast_slot;
	for (const auto &item : link_replies) 
		if (item.first < earliest_transmission)
			earliest_transmission = item.first;
	return earliest_transmission;
}

unsigned int SHLinkManager::getNextBeaconSlot() const {
	if (!next_beacon_scheduled || !this->beacon_module.isEnabled())
		throw std::runtime_error("SHLinkManager::getNextBeaconSlot for unscheduled beacon.");
	unsigned int beacon_slot = this->beacon_module.getNextBeaconSlot();
	if (current_reservation_table->getReservation(beacon_slot) != Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON))
		beacon_slot++; // if onSlotEnd() has recently been called, then the returned beacon_slot is off by one
	if (current_reservation_table->getReservation(beacon_slot) != Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON))
		throw std::runtime_error("SHLinkManager::getNextBeaconSlot cannot find beacon reservation.");
	return beacon_slot;
}

void SHLinkManager::broadcastCollisionDetected(const MacId& collider_id, Reservation::Action mark_as) {
	coutd << "re-scheduling broadcast from t=" << next_broadcast_slot << " to -> ";
	// remember current broadcast slot
	auto current_broadcast_slot = next_broadcast_slot;
	// unschedule it
	unscheduleBroadcastSlot();
	// mark it as BUSY so it won't be scheduled again
	current_reservation_table->mark(current_broadcast_slot, Reservation(collider_id, mark_as));
	// find a new slot
	try {
		scheduleBroadcastSlot();
		coutd << "next broadcast in " << next_broadcast_slot << " slots -> ";
	} catch (const std::exception &e) {
		throw std::runtime_error("Error when trying to re-schedule broadcast due to detected collision: " + std::string(e.what()));
	}		
	mac->statisticReportBroadcastCollisionDetected();
}

void SHLinkManager::beaconCollisionDetected(const MacId& collider_id, Reservation::Action mark_as) {
	auto current_beacon_slot = getNextBeaconSlot();
	coutd << "re-scheduling beacon from t=" << current_beacon_slot << " to -> ";		
	// unschedule it
	unscheduleBeaconSlot();
	// mark it as BUSY so it won't be scheduled again
	current_reservation_table->mark(current_beacon_slot, Reservation(collider_id, mark_as));
	// find a new slot
	try {
		scheduleBeacon();		
	} catch (const std::exception &e) {
		throw std::runtime_error("Error when trying to re-schedule beacon due to detected collision detected: " + std::string(e.what()));
	}		
	mac->statisticReportBeaconCollisionDetected();
}

void SHLinkManager::reportThirdPartyExpectedLinkReply(int slot_offset, const MacId& sender_id) {
	coutd << "marking slot in " << slot_offset << " as RX@" << sender_id << " (expecting a third-party link reply there) -> ";
	const auto &res = current_reservation_table->getReservation(slot_offset);
	// check if own transmissions clash with it
	if (res.isTx()) {		
		coutd << "re-scheduling own scheduled broadcast -> ";
		broadcastCollisionDetected(sender_id, Reservation::RX);		
	} else if (res.isBeaconTx()) {
		coutd << "re-scheduling own scheduled beacon -> ";
		beaconCollisionDetected(sender_id, Reservation::RX);
	} else {
		// overwrite any other reservations
		coutd << res << "->";
		current_reservation_table->mark(slot_offset, Reservation(sender_id, Reservation::Action::RX));
		coutd << current_reservation_table->getReservation(slot_offset) << " -> ";			
	}	
}

void SHLinkManager::processBroadcastMessage(const MacId& origin, L2HeaderBroadcast*& header) {
	mac->statisticReportBroadcastMessageProcessed();
}

void SHLinkManager::processUnicastMessage(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	// TODO compare to local ID, discard or forward resp.
	LinkManager::processUnicastMessage(header, payload);
}

void SHLinkManager::processBaseMessage(L2HeaderBase*& header) {	
	// Check indicated next broadcast slot.
	int next_broadcast = (int) header->burst_offset;
	if (next_broadcast > 0) { // If it has been set ...
		// ... check local reservation
		const Reservation& res = current_reservation_table->getReservation(next_broadcast);
		// if locally the slot is IDLE, then schedule listening to this broadcast
		if (res.isIdle()) {
			current_reservation_table->mark(next_broadcast, Reservation(header->src_id, Reservation::RX));
			coutd << "marked next broadcast in " << next_broadcast << " slots as RX -> ";
		// if locally, one's own transmission is scheduled...
		} else if (res.isTx()) {
			coutd << "detected collision with own broadcast in " << next_broadcast << " slots -> ";
			broadcastCollisionDetected(header->src_id, Reservation::RX);			
		// if locally, one's own beacon is scheduled...
		} else if (res.isBeaconTx()) {
			coutd << "detected collision with own beacon in " << next_broadcast << " slots -> ";
			beaconCollisionDetected(header->src_id, Reservation::RX);			
		} else {
			coutd << "indicated next broadcast in " << next_broadcast << " slots is locally reserved for " << res << " (not doing anything) -> ";
		}
	} else
		coutd << "no next broadcast slot indicated -> ";
}

void SHLinkManager::processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {	
	coutd << "forwarding link request to PPLinkManager -> ";
	// do NOT report the received request to the MAC, as the PPLinkManager will do that (otherwise it'll be counted twice)	
	((PPLinkManager*) mac->getLinkManager(origin_id))->processLinkRequestMessage(header, payload, origin_id);	
}

void SHLinkManager::processLinkReplyMessage(const L2HeaderLinkReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {	
	coutd << "forwarding link reply to PPLinkManager -> ";	
	((PPLinkManager*) mac->getLinkManager(origin_id))->processLinkReplyMessage(header, payload, origin_id);	
}

SHLinkManager::~SHLinkManager() {
	for (auto pair : link_requests) {
		delete pair.first;
		delete pair.second;
	}
	for (auto pair : link_replies) {
		delete pair.second.first;
		delete pair.second.second;
	}
}

void SHLinkManager::assign(const FrequencyChannel* channel) {
	LinkManager::assign(channel);
}

void SHLinkManager::onPacketReception(L2Packet*& packet) {
	const MacId& id = packet->getOrigin();			
	LinkManager::onPacketReception(packet);
}

void SHLinkManager::setTargetCollisionProb(double value) {
	this->broadcast_target_collision_prob = value;
}

void SHLinkManager::scheduleBeacon() {
	if (beacon_module.isEnabled()) {
		// un-schedule current beacon slot
		unscheduleBeaconSlot();
		// and schedule a new one
		try {
			int num_candidates = getNumCandidateSlots(this->broadcast_target_collision_prob, beacon_module.getMinBeaconCandidateSlots(), this->MAX_CANDIDATES);
			int next_beacon_slot = (int) beacon_module.scheduleNextBeacon(num_candidates, mac->getNeighborObserver().getNumActiveNeighbors(), current_reservation_table, reservation_manager->getTxTable());
			mac->statisticReportMinBeaconOffset((std::size_t) beacon_module.getBeaconOffset());
			if (!(current_reservation_table->isIdle(next_beacon_slot) || current_reservation_table->getReservation(next_beacon_slot).isBeaconTx())) {
				std::stringstream ss;
				ss << *mac << "::" << *this << "::scheduleBeacon scheduled a beacon slot at a non-idle resource: " << current_reservation_table->getReservation(next_beacon_slot) << "!";
				throw std::runtime_error(ss.str());
			}
			current_reservation_table->mark(next_beacon_slot, Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON));
			next_beacon_scheduled = true;
			coutd << *mac << "::" << *this << "::scheduleBeacon scheduled next beacon slot in " << next_beacon_slot << " slots (" << beacon_module.getMinBeaconCandidateSlots() << " candidates) -> ";					
		} catch (const std::exception &e) {
			coutd << "couldn't schedule a next beacon: " << e.what() << " -> ";
		}
	}
}

void SHLinkManager::unscheduleBeaconSlot() {	
	if (beacon_module.isEnabled() && next_beacon_scheduled) {		
		if (current_reservation_table == nullptr)
			throw std::runtime_error("SHLinkManager::unscheduleBeaconSlot for unset ReservationTable.");			
		int beacon_slot = getNextBeaconSlot();				
		current_reservation_table->mark(beacon_slot, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));		
		next_beacon_scheduled = false;
		beacon_module.reset();
	}
}

void SHLinkManager::setMinNumCandidateSlots(int value) {
	MIN_CANDIDATES = value;
	beacon_module.setMinBeaconCandidateSlots(value);
}

void SHLinkManager::setMaxNumCandidateSlots(int value) {
	MAX_CANDIDATES = value;	
}

void SHLinkManager::setAlwaysScheduleNextBroadcastSlot(bool value) {
	this->always_schedule_next_slot = value;
}

void SHLinkManager::setUseContentionMethod(ContentionMethod method) {
	contention_method = method;
}

unsigned int SHLinkManager::getAvgNumSlotsInbetweenPacketGeneration() const {
	return (unsigned int) std::ceil(avg_num_slots_inbetween_packet_generations.get());
}

void SHLinkManager::setMinBeaconInterval(unsigned int value) {
	this->beacon_module.setMinBeaconInterval(value);
}

void SHLinkManager::setMaxBeaconInterval(unsigned int value) {
	this->beacon_module.setMaxBeaconInterval(value);
}

void SHLinkManager::setWriteResourceUtilizationIntoBeacon(bool flag) {
	this->beacon_module.setWriteResourceUtilizationIntoBeacon(flag);
}

void SHLinkManager::setEnableBeacons(bool flag) {
	this->beacon_module.setEnabled(flag);
}

void SHLinkManager::setAdvertiseNextSlotInCurrentHeader(bool flag) {
	this->advertise_slot_in_header = flag;
}