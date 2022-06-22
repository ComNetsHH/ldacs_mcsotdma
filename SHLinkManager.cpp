//
// Created by seba on 2/18/21.
//

#include <sstream>
#include "SHLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
// #include "PPLinkManager.hpp"
#include "LinkProposalFinder.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

SHLinkManager::SHLinkManager(ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int min_beacon_gap)
: LinkManager(SYMBOLIC_LINK_ID_BROADCAST, reservation_manager, mac), avg_num_slots_inbetween_packet_generations(100) {	
}

void SHLinkManager::onReceptionReservation() {

}

L2Packet* SHLinkManager::onTransmissionReservation() {
	coutd << *mac << "::" << *this << "::onTransmissionReservation -> ";	
	size_t capacity = mac->getCurrentDatarate();
	coutd << "requesting " << capacity << " bits from upper layer -> ";

	// request data
	L2Packet *packet = mac->requestSegment(capacity, link_id);
	L2HeaderSH *header = (L2HeaderSH*) packet->getHeaders().at(0);	
	coutd << "got " << packet->getBits() << "-bit packet -> ";		
	assert(packet->getBits() <= capacity && "got more bits than I asked for");		
	capacity -= packet->getBits();

	// schedule next slot and write offset into header
	coutd << "scheduling next broadcast slot -> ";
	try {
		scheduleBroadcastSlot();
		coutd << "next broadcast in " << next_broadcast_slot << " slots -> ";
		// Put it into the header.
		if (this->advertise_slot_in_header) {
			coutd << "advertising slot in header -> ";
			header->slot_offset = next_broadcast_slot;
		}
	} catch (const std::exception &e) {
		throw std::runtime_error("Error when trying to schedule next broadcast because there's more data: " + std::string(e.what()));
	}	

	// add link requests
	coutd << "considering " << link_requests.size() << " pending link requests: ";
	for (auto dest_id : link_requests) {		
		coutd << "id=" << dest_id << " -> ";
		// check if we know preferred links
		const auto &advertised_normalized_proposals = mac->getNeighborObserver().getAdvertisedLinkProposals(dest_id, mac->getCurrentSlot());
		coutd << advertised_normalized_proposals.size() << " proposals -> ";
		// propose locally-usable links if no proposals are saved or none are valid
		bool must_propose_something_new = advertised_normalized_proposals.empty(); // || !anyProposalValid(advertised_normalized_proposals);
		std::vector<LinkProposal> link_proposals;
		if (must_propose_something_new) {
			coutd << "finding locally-usable links -> ";
			mac->statisticReportSentOwnProposals();

			size_t num_proposals = 3;
			auto contributions_and_timeouts = mac->getUsedPPDutyCycleBudget();
			const std::vector<double> &used_pp_duty_cycle_budget = contributions_and_timeouts.first;
			const std::vector<int> &remaining_pp_timeouts = contributions_and_timeouts.second;
			double sh_budget = mac->getDutyCycle().getSHBudget(used_pp_duty_cycle_budget);
			auto pair = mac->getDutyCycle().getPeriodicityPP(used_pp_duty_cycle_budget, remaining_pp_timeouts, sh_budget, next_broadcast_slot);	
			int min_offset = pair.first;
			int min_period = pair.second;			
			unsigned int min_time_slot_offset;
			try {
				// the proposal should be after the other user's next broadcast slot 
				min_time_slot_offset = mac->getNeighborObserver().getNextExpectedBroadcastSlotOffset(dest_id);
				coutd << "using saved neighbor's next broadcast in " << min_time_slot_offset << " slots as minimum offset -> ";
			} catch (const std::exception &e) {
				// if that is unknown, use own next broadcast slot
				min_time_slot_offset = next_broadcast_slot;
				coutd << "using own next broadcast in " << min_time_slot_offset << " slots as minimum offset -> ";
			}
			link_proposals = LinkProposalFinder::findLinkProposals(num_proposals, min_time_slot_offset, min_period, 1, mac->getDefaultPpLinkTimeout(), mac->shouldLearnDmeActivity(), mac->getReservationManager(), mac);			
		// propose advertised links if we know of some
		} else {
			mac->statisticReportSentSavedProposals();
			throw std::runtime_error("using saved proposals not yet implemented");
		}
		coutd << "determined " << link_proposals.size() << " link proposals -> ";
		for (auto proposal : link_proposals) {
			// save request
			header->link_requests.push_back(L2HeaderSH::LinkRequest(dest_id, proposal));			
			// lock resources
			
		}
	}

	// find link proposals
	size_t num_proposals = 3;
	auto contributions_and_timeouts = mac->getUsedPPDutyCycleBudget();
	const std::vector<double> &used_pp_duty_cycle_budget = contributions_and_timeouts.first;
	const std::vector<int> &remaining_pp_timeouts = contributions_and_timeouts.second;	
	double sh_budget = mac->getDutyCycle().getSHBudget(used_pp_duty_cycle_budget);
	auto pair = mac->getDutyCycle().getPeriodicityPP(used_pp_duty_cycle_budget, remaining_pp_timeouts, sh_budget, next_broadcast_slot);	
	int min_offset = pair.first;
	int min_period = pair.second;
	std::vector<LinkProposal> proposable_links = LinkProposalFinder::findLinkProposals(num_proposals, next_broadcast_slot, min_period, 1, mac->getDefaultPpLinkTimeout(), mac->shouldLearnDmeActivity(), mac->getReservationManager(), mac);
	// write proposals into header
	for (const LinkProposal &proposal : proposable_links) 
		header->link_proposals.push_back(L2HeaderSH::LinkProposalMessage(proposal));	
	coutd << "wrote " << header->link_proposals.size() << " link proposals into header -> ";	

	// discard empty packet
	if (packet->empty()) {		
		mac->statisticReportWastedSHTransmission();
		delete packet;
		return nullptr;	
	}
	mac->statisticReportBroadcastSent();
	mac->statisticReportBroadcastMacDelay(measureMacDelay());				
	return packet;	
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
	
	LinkManager::onSlotEnd();
}

void SHLinkManager::sendLinkRequest(const MacId &dest_id) {	
	coutd << *this << " will send link request to " << dest_id << " with next transmission -> ";	
	// save request
	link_requests.push_back(dest_id);	
	// schedule broadcast slot if necessary
	notifyOutgoing(1);
}

// size_t SHLinkManager::cancelLinkRequest(const MacId& id) {
// 	size_t num_removed = 0;
// 	for (auto it = link_requests.begin(); it != link_requests.end();) {
// 		const auto* header = it->first;
// 		if (header->getDestId() == id) {
// 			delete (*it).first;
// 			delete (*it).second;
// 			it = link_requests.erase(it);
// 			num_removed++;
// 		} else
// 			it++;
// 	}
// 	return num_removed;
// }

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
		// coutd << "printing reservations over entire planning horizon: " << std::endl << "t\tlocal\t\tTX" << std::endl;
		// for (size_t t = 0; t < current_reservation_table->getPlanningHorizon(); t++) 
		// 	coutd << "t=" << t << ":\t" << current_reservation_table->getReservation(t) << "\t" << reservation_manager->getTxTable()->getReservation(t) << std::endl;
		throw std::runtime_error("SHLinkManager::broadcastSlotSelection found zero candidate slots at min_offset=" + std::to_string(min_offset));
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
	// Compute minimum slot offset to adhere to duty cycle.
	auto contributions_and_timeouts = mac->getUsedPPDutyCycleBudget();
	const std::vector<double> &used_pp_duty_cycle_budget = contributions_and_timeouts.first;
	int min_offset = mac->getDutyCycle().getOffsetSH(used_pp_duty_cycle_budget);	
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

// void SHLinkManager::processBeaconMessage(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
// 	coutd << "parsing incoming beacon -> ";
// 	// auto pair = beacon_module.parseBeacon(origin_id, (const BeaconPayload*&) payload, reservation_manager);
// 	// if (pair.first) {
// 	// 	beaconCollisionDetected(origin_id, Reservation::RX);		
// 	// } if (pair.second) {
// 	// 	broadcastCollisionDetected(origin_id, Reservation::RX);
// 	// }
// 	// pass it to the MAC layer
// 	mac->onBeaconReception(origin_id, L2HeaderBeacon(*header));
// }

bool SHLinkManager::isNextBroadcastScheduled() const {
	return next_broadcast_scheduled;
}

unsigned int SHLinkManager::getNextBroadcastSlot() const {
	return next_broadcast_slot;	
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

void SHLinkManager::reportThirdPartyExpectedLinkReply(int slot_offset, const MacId& sender_id) {
	coutd << "marking slot in " << slot_offset << " as RX@" << sender_id << " (expecting a third-party link reply there) -> ";
	const auto &res = current_reservation_table->getReservation(slot_offset);
	// check if own transmissions clash with it
	if (res.isTx()) {		
		coutd << "re-scheduling own scheduled broadcast -> ";
		broadcastCollisionDetected(sender_id, Reservation::RX);		
	} else if (res.isBeaconTx()) {
		coutd << "re-scheduling own scheduled beacon -> ";
		// beaconCollisionDetected(sender_id, Reservation::RX);
	} else {
		// overwrite any other reservations
		coutd << res << "->";
		current_reservation_table->mark(slot_offset, Reservation(sender_id, Reservation::Action::RX));
		coutd << current_reservation_table->getReservation(slot_offset) << " -> ";			
	}	
}

void SHLinkManager::processBroadcastMessage(const MacId& origin, L2HeaderSH*& header) {
	mac->statisticReportBroadcastMessageProcessed();

	// check advertised next transmission slot
	unsigned int advertised_broadcast_slot = header->slot_offset;
	if (advertised_broadcast_slot > 0) { // If it has been set ...
		// remember the advertised slot offset
		mac->reportBroadcastSlotAdvertisement(origin, advertised_broadcast_slot);
		// ... check local reservation
		const Reservation& res = current_reservation_table->getReservation(advertised_broadcast_slot);
		// if locally the slot is IDLE, then schedule listening to this broadcast
		if (res.isIdle()) {
			current_reservation_table->mark(advertised_broadcast_slot, Reservation(header->src_id, Reservation::RX));
			coutd << "marked next broadcast in " << advertised_broadcast_slot << " slots as RX -> ";
		// if locally, one's own transmission is scheduled...
		} else if (res.isTx()) {
			coutd << "detected collision with own broadcast in " << advertised_broadcast_slot << " slots -> ";
			broadcastCollisionDetected(header->src_id, Reservation::RX);							
		} else {
			coutd << "indicated next broadcast in " << advertised_broadcast_slot << " slots is locally reserved for " << res << " (not doing anything) -> ";
		}
	} else
		coutd << "no next broadcast slot indicated -> ";

	// save link proposals		
	coutd << "saving " << header->link_proposals.size() << " advertised link proposals -> ";
	if (!header->link_proposals.empty()) {
		mac->getNeighborObserver().clearAdvertisedLinkProposals(header->src_id);
		for (const auto &proposal : header->link_proposals) 
			mac->getNeighborObserver().addAdvertisedLinkProposal(header->src_id, mac->getCurrentSlot(), proposal.proposed_link);	
	}
}

// void SHLinkManager::processUnicastMessage(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
// 	// TODO compare to local ID, discard or forward resp.
// 	LinkManager::processUnicastMessage(header, payload);
// }

// void SHLinkManager::processBaseMessage(L2HeaderBase*& header) {	
// 	// Check indicated next broadcast slot.
// 	int next_broadcast = (int) header->burst_offset;
// 	if (next_broadcast > 0) { // If it has been set ...
// 		// ... check local reservation
// 		const Reservation& res = current_reservation_table->getReservation(next_broadcast);
// 		// if locally the slot is IDLE, then schedule listening to this broadcast
// 		if (res.isIdle()) {
// 			current_reservation_table->mark(next_broadcast, Reservation(header->src_id, Reservation::RX));
// 			coutd << "marked next broadcast in " << next_broadcast << " slots as RX -> ";
// 		// if locally, one's own transmission is scheduled...
// 		} else if (res.isTx()) {
// 			coutd << "detected collision with own broadcast in " << next_broadcast << " slots -> ";
// 			broadcastCollisionDetected(header->src_id, Reservation::RX);			
// 		// if locally, one's own beacon is scheduled...
// 		} else if (res.isBeaconTx()) {
// 			coutd << "detected collision with own beacon in " << next_broadcast << " slots -> ";
// 			throw std::runtime_error("beacon collision handling not implemented");
// 			// beaconCollisionDetected(header->src_id, Reservation::RX);			
// 		} else {
// 			coutd << "indicated next broadcast in " << next_broadcast << " slots is locally reserved for " << res << " (not doing anything) -> ";
// 		}
// 	} else
// 		coutd << "no next broadcast slot indicated -> ";
// }

// void SHLinkManager::processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {	
// 	coutd << "forwarding link request to PPLinkManager -> ";
// 	// do NOT report the received request to the MAC, as the PPLinkManager will do that (otherwise it'll be counted twice)	
// 	((PPLinkManager*) mac->getLinkManager(origin_id))->processLinkRequestMessage(header, payload, origin_id);	
// }

// void SHLinkManager::processLinkReplyMessage(const L2HeaderLinkReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {	
// 	coutd << "forwarding link reply to PPLinkManager -> ";	
// 	((PPLinkManager*) mac->getLinkManager(origin_id))->processLinkReplyMessage(header, payload, origin_id);	
// }

SHLinkManager::~SHLinkManager() {	
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

void SHLinkManager::setMinNumCandidateSlots(int value) {
	MIN_CANDIDATES = value;	
}

void SHLinkManager::setMaxNumCandidateSlots(int value) {
	MAX_CANDIDATES = value;	
}

void SHLinkManager::setUseContentionMethod(ContentionMethod method) {
	contention_method = method;
}

unsigned int SHLinkManager::getAvgNumSlotsInbetweenPacketGeneration() const {
	return (unsigned int) std::ceil(avg_num_slots_inbetween_packet_generations.get());
}

void SHLinkManager::setAdvertiseNextSlotInCurrentHeader(bool flag) {
	this->advertise_slot_in_header = flag;
}

double SHLinkManager::getNumTxPerTimeSlot() const {
	if (!next_broadcast_scheduled)
		return 0.0;
	double num_broadcasts = next_broadcast_scheduled && next_broadcast_slot > 0 ? 1.0/((double) next_broadcast_slot) : 0.0;	
	return num_broadcasts;
}

bool SHLinkManager::isActive() const {
	return next_broadcast_scheduled;
}