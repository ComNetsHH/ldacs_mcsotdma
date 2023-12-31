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

#include <sstream>
#include "SHLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "PPLinkManager.hpp"
#include "LinkProposalFinder.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

SHLinkManager::SHLinkManager(ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int min_beacon_gap)
: LinkManager(SYMBOLIC_LINK_ID_BROADCAST, reservation_manager, mac), avg_num_slots_inbetween_packet_generations(100) {	
	should_report_missing_packets_to_arq = false;
}

L2Packet* SHLinkManager::onTransmissionReservation() {
	coutd << *mac << "::" << *this << "::onTransmissionReservation -> ";	
	size_t capacity = mac->getCurrentDatarate();
	coutd << "requesting " << capacity << " bits from upper layer -> ";

	// request data
	L2Packet *packet = mac->requestSegment(capacity, link_id);
	L2HeaderSH *&header = (L2HeaderSH*&) packet->getHeaders().at(0);		
	coutd << "got " << packet->getBits() << "-bit packet -> ";		
	assert(packet->getBits() <= capacity && "got more bits than I asked for");		
	capacity -= packet->getBits();

	// write source iD
	header->src_id = mac->getMacId();	

	// add link requests
	if (!link_requests.empty())
		coutd << "considering " << link_requests.size() << " pending link requests: ";
	for (auto pair : link_requests) {		
		MacId &dest_id = pair.first;	
		uint64_t &generation_time = pair.second;	
		coutd << "id=" << dest_id << " -> ";
		// check if we know preferred links
		const auto &advertised_normalized_proposals = mac->getNeighborObserver().getAdvertisedLinkProposals(dest_id, mac->getCurrentSlot());
		coutd << advertised_normalized_proposals.size() << " proposals -> ";

		// propose locally-usable links if no proposals are saved or none are valid		
		bool must_propose_something_new = advertised_normalized_proposals.empty(); // || !anyProposalValid(advertised_normalized_proposals);
		std::vector<LinkProposal> link_proposals;
		
		int period, min_offset;
		try {
			period = getPPMinOffsetAndPeriod().second;		
		} catch (const no_duty_cycle_budget_left_error &e) {
			std::stringstream ss;
			ss << *mac << "::" << *this << "::onTransmissionReservation error processing link request: " << e.what();
			throw std::runtime_error(std::string(e.what()));
		}			

		int num_forward_bursts = 1, num_reverse_bursts = 1;
		bool scheduled_broadcast_slot = false;
		// propose something that works locally
		if (must_propose_something_new) {
			mac->statisticReportSentOwnProposals();
			coutd << "finding locally-usable links -> ";			
			auto pair = proposeLocalLinks(dest_id, num_forward_bursts, num_reverse_bursts, num_proposals_unadvertised_link_requests);			
			link_proposals = pair.first;
			min_offset = pair.second;
		// propose advertised links if we know of some
		} else {
			coutd << "selecting remote-advertised links -> ";			
			try {
				link_proposals.push_back(proposeRemoteLinks(dest_id, num_forward_bursts, num_reverse_bursts));			
				min_offset = mac->getNeighborObserver().getNextExpectedBroadcastSlotOffset(dest_id);
				mac->statisticReportSentSavedProposals();
			// fall-back to locally-usable links of the advertised ones don't fit
			} catch (const std::exception &e) {
				mac->statisticReportSentOwnProposals();
				coutd << "finding locally-usable links instead -> ";				
				auto pair = proposeLocalLinks(dest_id, num_forward_bursts, num_reverse_bursts, num_proposals_unadvertised_link_requests);			
				link_proposals = pair.first;
				min_offset = pair.second;
			}
		}
		coutd << "determined " << link_proposals.size() << " link proposals -> ";
		coutd.flush();
		if (!link_proposals.empty()) {			
			bool notified_pp = false;
			for (auto proposal : link_proposals) {
				// save request
				header->link_requests.push_back(L2HeaderSH::LinkRequest(dest_id, proposal, generation_time));			
				// notify PP
				auto *pp = (PPLinkManager*) mac->getLinkManager(dest_id);
				if (!notified_pp) {
					notified_pp = true;					
					// this resets the locked resource map so must be called before lockProposedResources
					pp->notifyLinkRequestSent(num_forward_bursts, num_reverse_bursts, period, min_offset, min_offset);
				}
				// lock resources				
				pp->lockProposedResources(proposal);				
			}
			mac->statisticReportLinkRequestSent();		
		} else {
			coutd << "empty proposals, couldn't propose links during link request -> ";
		}
	}
	// remove link requests that have been added
	if (!header->link_requests.empty()) {
		for (auto &request : header->link_requests) {
			MacId dest_id = request.dest_id;
			cancelLinkRequest(dest_id);
		}
	}

	// schedule next slot and write offset into header	
	try {
		if (next_broadcast_slot == 0) {// could be that proposeLocalLinks already scheduled the next slot
			coutd << "scheduling next broadcast slot -> ";
			scheduleBroadcastSlot();	
		} else
			coutd << "next broadcast slot has already been scheduled -> ";
		// Put it into the header.
		if (this->advertise_slot_in_header) {			
			header->slot_offset = next_broadcast_slot;
			coutd << "advertising next broadcast in " << header->slot_offset << " slots -> ";
		}
	} catch (const std::exception &e) {
		throw std::runtime_error("Error when trying to schedule next broadcast: " + std::string(e.what()));
	}	

	// attach next link reply
	if (!link_replies.empty()) {
		auto reply = link_replies.at(0);
		coutd << "attaching link reply for " << reply.dest_id << " -> ";
		header->link_reply = L2HeaderSH::LinkReply(reply);		
		link_replies.erase(link_replies.begin());
		if (link_replies.empty())
			coutd << "no more replies pending -> ";
		else 
			coutd << link_replies.size() << " replies pending -> ";
		mac->statisticReportLinkReplySent();
		// check if pending link replies are out-of-date now
	}

	// find link proposals	
	size_t num_proposals = 3;
	coutd << "computing " << num_proposals << " proposals -> ";
	int min_offset, period;
	try {
		auto pair = getPPMinOffsetAndPeriod();		
		min_offset = pair.first; 
		period = pair.second;
	} catch (const no_duty_cycle_budget_left_error &e) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::onTransmissionReservation error processing link request: " << e.what();
		throw std::runtime_error(std::string(e.what()));
	}				
	int num_forward_bursts = 1, num_reverse_bursts = 1;	
	std::vector<LinkProposal> proposable_links = LinkProposalFinder::findLinkProposals(num_proposals, next_broadcast_slot, num_forward_bursts, num_reverse_bursts, period, mac->getDefaultPPLinkTimeout(), mac->shouldLearnDmeActivity(), mac->getReservationManager(), mac);
	// write proposals into header
	for (const LinkProposal &proposal : proposable_links) 
		header->link_proposals.push_back(L2HeaderSH::LinkProposalMessage(proposal));	
	coutd << "wrote " << header->link_proposals.size() << " link proposals into header -> ";		

	// write utilizations into header
	header->link_utilizations = mac->getPPLinkUtilizations();

	// transmit packet
	mac->statisticReportBroadcastSent();
	mac->statisticReportBroadcastMacDelay(measureMacDelay());				
	return packet;	
}

size_t SHLinkManager::cancelLinkRequest(const MacId& id) {
	size_t num_cancelled = 0;
	for (auto it = link_requests.begin(); it != link_requests.end();) {
		if ((*it).first == id) {
			it = link_requests.erase(it);
			num_cancelled++;
		} else
			it++;
	}
	return num_cancelled;
}
size_t SHLinkManager::cancelLinkReply(const MacId& id) {	
	size_t num_cancelled = 0;
	for (auto it = link_replies.begin(); it != link_replies.end();) {		
		if ((*it).dest_id == id) {
			it = link_replies.erase(it);
			num_cancelled++;
		} else
			it++;
	}
	return num_cancelled;
}

std::pair<std::vector<LinkProposal>, int> SHLinkManager::proposeLocalLinks(const MacId& dest_id, int num_forward_bursts, int num_reverse_bursts, size_t num_proposals) {	
	auto contributions_and_timeouts = mac->getUsedPPDutyCycleBudget();
	const std::vector<double> &used_pp_duty_cycle_budget = contributions_and_timeouts.first;
	const std::vector<int> &remaining_pp_timeouts = contributions_and_timeouts.second;
	double sh_budget = mac->shouldConsiderDutyCycle() ? mac->getDutyCycle().getSHBudget(used_pp_duty_cycle_budget) : 1.0;
	coutd << "duty cycle considerations: sh_budget=" << sh_budget*100 << "% -> ";
	int min_offset;
	int period;
	try {
		auto pair = mac->getDutyCycle().getPeriodicityPP(used_pp_duty_cycle_budget, remaining_pp_timeouts, sh_budget, next_broadcast_slot);	
		min_offset = pair.first;						
		period = pair.second;	
	} catch (const no_duty_cycle_budget_left_error &e) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::proposeLocalLinks error: " << e.what();
		throw std::runtime_error(std::string(e.what()));
	}	
	coutd << " min_period=" << period << " -> ";			
	try {
		// the proposal should be after the other user's next broadcast slot 
		int next_expected_broadcast = mac->getNeighborObserver().getNextExpectedBroadcastSlotOffset(dest_id);
		min_offset = std::max(min_offset, next_expected_broadcast + 1);
		coutd << "using saved neighbor's next broadcast in " << min_offset << " slots as minimum offset -> ";
	} catch (const std::exception &e) {
		// if that is unknown, use own next broadcast slot
		if (next_broadcast_slot == 0) 
			scheduleBroadcastSlot();
		min_offset = std::max(min_offset, (int) next_broadcast_slot);
		coutd << "using own next broadcast in " << min_offset << " slots as minimum offset -> ";
	}			
	return {LinkProposalFinder::findLinkProposals(num_proposals, min_offset, num_forward_bursts, num_reverse_bursts, period, mac->getDefaultPPLinkTimeout(), mac->shouldLearnDmeActivity(), mac->getReservationManager(), mac), min_offset};			
}

LinkProposal SHLinkManager::proposeRemoteLinks(const MacId& dest_id, int num_forward_bursts, int num_reverse_bursts) {
	// find advertised links
	std::vector<LinkProposal> advertisements = mac->getNeighborObserver().getAdvertisedLinkProposals(dest_id, mac->getCurrentSlot());
	coutd << "checking " << advertisements.size() << " advertised links -> ";
	// compare to local reservations
	std::vector<LinkProposal> valid_links;
	for (const auto possible_link : advertisements) {
		const ReservationTable *table = reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(possible_link.center_frequency));
		bool is_link_initiator = true;
		bool is_valid = table->isLinkValid(possible_link.slot_offset, possible_link.period, num_forward_bursts, num_reverse_bursts, mac->getDefaultPPLinkTimeout(), is_link_initiator);
		coutd << "link at t=" << possible_link.slot_offset << "@" << possible_link.center_frequency << "kHz is " << (is_valid ? "valid" : "invalid") << " -> ";
		if (is_valid)
			valid_links.push_back(possible_link);
	}		
	if (valid_links.empty())
		throw std::runtime_error("SHLinkManager::proposeRemoteLinks couldn't find any valid links");
	// select earliest suitable
	LinkProposal earliest_link;
	int earliest_offset = (int) reservation_manager->getPlanningHorizon();
	for (const auto valid_link : valid_links) {
		if (valid_link.slot_offset < earliest_offset) {
			earliest_link = valid_link;
			earliest_offset = valid_link.slot_offset;
		}
	}
	coutd << "earliest link is at t=" << earliest_link.slot_offset << "@" << earliest_link.center_frequency << "kHz -> ";
	return earliest_link;
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
	LinkManager::onSlotStart(num_slots);
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
	} else {
		coutd << "scheduling next broadcast slot -> ";	
		scheduleBroadcastSlot();
	}

	// broadcast link manager should always have a ReservationTable assigned	
	assert(current_reservation_table != nullptr && "SHLinkManager::onSlotStart for unset ReservationTable.");

	// mark reception slot if there's nothing else to do
	const auto& current_reservation = current_reservation_table->getReservation(0);
	if (current_reservation.isIdle() || current_reservation.isBusy()) {
		coutd << "marking SH reception -> ";
		try {
			current_reservation_table->mark(0, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::RX));
		} catch (const std::exception& e) {
			throw std::runtime_error("SHLinkManager::onSlotStart(" + std::to_string(num_slots) + ") error trying to mark BC reception slot: " + e.what());
		}
	}
}

void SHLinkManager::onSlotEnd() {
	LinkManager::onSlotEnd();
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
	link_requests.push_back({dest_id, mac->getCurrentSlot()});	
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
	// Don't make use of contention estimation in any way. Just select something out of the next MIN_CANDIDATES idle slots.
	} else if (contention_method == ContentionMethod::naive_random_access) {
		k = this->MIN_CANDIDATES;
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
	if (!this->do_transmit) {
		coutd << "configured not to transmit, cancelling broadcast slot selection -> ";
		return;
	}
	unscheduleBroadcastSlot();
	// Compute minimum slot offset to adhere to duty cycle.
	auto contributions_and_timeouts = mac->getUsedPPDutyCycleBudget();
	const std::vector<double> &used_pp_duty_cycle_budget = contributions_and_timeouts.first;
	int min_offset = mac->shouldConsiderDutyCycle() ? mac->getDutyCycle().getOffsetSH(used_pp_duty_cycle_budget) : 1;	
	if (uint32_t(min_offset) > reservation_manager->getPlanningHorizon() || min_offset < 0) {
		std::stringstream ss;
		ss << *mac << "::" << *this << " computed min_offset=" << min_offset << " at planning_horizon=" << reservation_manager->getPlanningHorizon();
		if (mac->shouldConsiderDutyCycle()) {
			ss << " considering duty cycle with PP contributions of [";
			for (auto d : used_pp_duty_cycle_budget)
				ss << d << ", ";
			ss << "] with link stati: ";
			for (const auto &item : mac->getLinkManagers()) {
				if (item.first != SYMBOLIC_LINK_ID_BROADCAST && item.first != SYMBOLIC_LINK_ID_BEACON) {
					const PPLinkManager *pp = (const PPLinkManager*) item.second;
					ss << *pp << " " << pp->getLinkStatus() << std::endl;
				}
			}
		} else
			ss << " not considering duty cycle";
		throw std::runtime_error(ss.str());
	}
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
	mac->reportNeighborActivity(origin);

	// check advertised next transmission slot	
	if (header->slot_offset > 0) { // If it has been set ...
		coutd << "checking advertised next broadcast slot in " << header->slot_offset << " slots -> ";
		// remember the advertised slot offset
		mac->reportBroadcastSlotAdvertisement(origin, header->slot_offset);
		// ... check local reservation				
		const Reservation& res = current_reservation_table->getReservation(header->slot_offset);		
		// if locally the slot is IDLE, then schedule listening to this broadcast
		if (res.isIdle()) {
			current_reservation_table->mark(header->slot_offset, Reservation(header->src_id, Reservation::RX));
			coutd << "marked next broadcast in " << header->slot_offset << " slots as RX -> ";
		// if locally, one's own transmission is scheduled...
		} else if (res.isTx()) {
			coutd << "detected collision with own broadcast in " << header->slot_offset << " slots -> ";
			broadcastCollisionDetected(header->src_id, Reservation::RX);							
		} else {
			coutd << "indicated next broadcast in " << header->slot_offset << " slots is locally reserved for " << res << " (not doing anything) -> ";
		}
	} else
		coutd << "no next broadcast slot indicated -> ";

	// save link proposals		
	if (!header->link_proposals.empty()) {
		coutd << "saving " << header->link_proposals.size() << " advertised link proposals -> ";	
		mac->getNeighborObserver().clearAdvertisedLinkProposals(header->src_id);		
		for (const auto &proposal : header->link_proposals) 
			mac->getNeighborObserver().addAdvertisedLinkProposal(header->src_id, mac->getCurrentSlot(), proposal.proposed_link);	
	}	

	// check link requests
	std::vector<std::pair<LinkProposal, uint64_t>> acceptable_links;
	bool received_request = false;
	if (!header->link_requests.empty())
		coutd << "processing " << header->link_requests.size() << " link requests -> ";	
	std::map<MacId, bool> already_processed_third_party_link_request = std::map<MacId, bool>();
	for (const auto &link_request : header->link_requests) {
		const MacId &dest_id = link_request.dest_id;
		// destined to us?
		if (dest_id == mac->getMacId()) {
			mac->statisticReportLinkRequestReceived();
			received_request = true;
			const auto &proposal = link_request.proposed_link;			
			// check if slot offset is large enough to reply in time
			if (link_request.proposed_link.slot_offset <= next_broadcast_slot) {
				coutd << "t=" << link_request.proposed_link.slot_offset << " would be before my next SH transmission at t=" << next_broadcast_slot << " -> NOT acceptable -> ";
				mac->statisticReportLinkRequestRejectedDueToUnacceptableReplySlot();
				continue;
			}			
			// check if any proposed link works locally
			const ReservationTable *table = reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(proposal.center_frequency));			
			bool is_link_initiator = false;
			bool is_acceptable = table->isLinkValid(proposal.slot_offset, proposal.period, proposal.num_tx_initiator, proposal.num_tx_recipient, mac->getDefaultPPLinkTimeout(), is_link_initiator) && isPPLinkDutyCycleConformant(proposal);
			if (is_acceptable) {
				coutd << "t=" << proposal.slot_offset << "@" << proposal.center_frequency << "kHz is acceptable -> ";
				acceptable_links.push_back({proposal, link_request.generation_time});
			} else
				coutd << "t=" << proposal.slot_offset << "@" << proposal.center_frequency << "kHz is NOT acceptable -> ";
		// destined to another user?
		} else {
			coutd << "passing link request on to 3rd-party-link -> ";
			coutd.flush();
			mac->statisticReportThirdPartyLinkRequestReceived();
			const MacId &src_id = header->src_id;
			auto &third_party_link = mac->getThirdPartyLink(src_id, dest_id);
			// reset previous locks and schedules for first link request to this destination
			auto it = already_processed_third_party_link_request.find(dest_id);
			if (it == already_processed_third_party_link_request.end() || ((*it).second == false)) {
				coutd << "resetting 3rd party link -> ";
				third_party_link.reset();
			}
			third_party_link.processLinkRequestMessage(link_request);
			already_processed_third_party_link_request[dest_id] = true;
		}
	}	

	if (received_request) {
		auto *pp = (PPLinkManager*) mac->getLinkManager(header->src_id);
		// accept if possible
		if (!acceptable_links.empty()) {
			LinkProposal earliest_link;
			int earliest_start_slot = reservation_manager->getPlanningHorizon();
			uint64_t generation_time;
			for (auto pair : acceptable_links) {
				auto link = pair.first;				 
				if (link.slot_offset < earliest_start_slot) {
					earliest_start_slot = link.slot_offset;
					earliest_link = link;
					generation_time = pair.second;
				}
			}
			pp->acceptLink(earliest_link, true, generation_time);
			size_t num_cancelled_own_link_requests = cancelLinkRequest(header->src_id);
			coutd << (num_cancelled_own_link_requests > 0 ? "cancelled own link request -> " : "");
			// write link reply			
			LinkProposal normalized_proposal = LinkProposal(earliest_link);
			normalized_proposal.slot_offset -= next_broadcast_slot;
			coutd << "will attach link reply to next SH transmission with normalized offset t=" << normalized_proposal.slot_offset << " -> ";			
			link_replies.push_back(L2HeaderSH::LinkReply(header->src_id, normalized_proposal));
		// start own link establishment otherwise
		} else {
			coutd << "no link request could be accepted, starting own link establishment -> ";
			pp->notifyOutgoing(1);
		}		
	}

	// check link reply	
	if (header->link_reply.dest_id != SYMBOLIC_ID_UNSET) {
		if (header->link_reply.dest_id == mac->getMacId()) {
			coutd << "processing link reply -> ";
			const LinkProposal &link = header->link_reply.proposed_link;
			if (isPPLinkDutyCycleConformant(link)) {
				auto *pp = (PPLinkManager*) mac->getLinkManager(header->src_id);			
				pp->acceptLink(link, false, 0);
			} else {
				coutd << "rejecting because duty cycle would be violated -> ";
			}
			mac->statisticReportLinkReplyReceived();
		} else {			
			coutd << "passing link reply on to 3rd-party-link -> ";
			mac->statisticReportThirdPartyLinkReplyReceived();
			auto &third_party_link = mac->getThirdPartyLink(header->src_id, header->link_reply.dest_id);
			third_party_link.processLinkReplyMessage(header->link_reply, header->src_id);
		}
	}

	// check link utilizations
	coutd << "processing " << header->link_utilizations.size() << " link utilizations: ";
	for (const auto &utilization : header->link_utilizations) {
		coutd << "processing link utilization on f=" << utilization.center_frequency << " -> ";
		mac->statisticReportLinkUtilizationReceived();
		// TODO potential third party processing
	}

	mac->onBeaconReception(origin, header->position);
}

SHLinkManager::~SHLinkManager() {	
}

void SHLinkManager::assign(const FrequencyChannel* channel) {
	LinkManager::assign(channel);
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

std::pair<int, int> SHLinkManager::getPPMinOffsetAndPeriod() const {
	auto contributions_and_timeouts = mac->getUsedPPDutyCycleBudget();
	const std::vector<double> &used_pp_duty_cycle_budget = contributions_and_timeouts.first;
	const std::vector<int> &remaining_pp_timeouts = contributions_and_timeouts.second;	
	double sh_budget = mac->shouldConsiderDutyCycle() ? mac->getDutyCycle().getSHBudget(used_pp_duty_cycle_budget) : 1.0;
	try {
		auto pair = mac->getDutyCycle().getPeriodicityPP(used_pp_duty_cycle_budget, remaining_pp_timeouts, sh_budget, next_broadcast_slot);		
		int min_offset = pair.first;		
		int period = mac->shouldUseFixedPPPeriod() ? mac->getFixedPPPeriod() : pair.second;
		return {min_offset, period};
	} catch (const no_duty_cycle_budget_left_error &e) {
		throw no_duty_cycle_budget_left_error("error in getPPMinOffsetAndPeriod: " + std::string(e.what()));
	}
}

void SHLinkManager::setShouldTransmit(bool value) {
	this->do_transmit = value;
}

bool SHLinkManager::isPPLinkDutyCycleConformant(const LinkProposal &link_proposal) const {
	if (!mac->shouldConsiderDutyCycle())
		return true;
	auto contributions_and_timeouts = mac->getUsedPPDutyCycleBudget();
	const std::vector<double> &used_pp_duty_cycle_budget = contributions_and_timeouts.first;	
	double sh_budget = mac->getDutyCycle().getSHBudget(used_pp_duty_cycle_budget);		
	double sum_used_budget = sh_budget;
	for (auto d : used_pp_duty_cycle_budget)
		sum_used_budget += d;
	sum_used_budget += 1.0 / (10.0 * std::pow(2.0, link_proposal.period));
	return sum_used_budget <= mac->getDutyCycle().getTotalBudget();
}