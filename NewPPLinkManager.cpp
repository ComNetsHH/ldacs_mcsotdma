//
// Created by Sebastian Lindner on 12/21/21.
//

#include "NewPPLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "SHLinkManager.hpp"
#include <sstream>

using namespace TUHH_INTAIRNET_MCSOTDMA;

NewPPLinkManager::NewPPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : LinkManager(link_id, reservation_manager, mac) {}

void NewPPLinkManager::onReceptionBurstStart(unsigned int burst_length) {	
}

void NewPPLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {
	throw std::runtime_error("onReceptionBurst not implemented");
}

L2Packet* NewPPLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	return nullptr;
}

void NewPPLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {
	throw std::runtime_error("onTransmissionBurst not implemented");
}

void NewPPLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *mac << "::" << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	// trigger link establishment
	if (link_status == link_not_established) {
		coutd << "link not established -> triggering establishment -> ";		
		establishLink();
	// unless it's already underway/established
	} else 
		coutd << "link status is '" << link_status << "' -> nothing to do." << std::endl;
	// update the traffic estimate	
	outgoing_traffic_estimate.put(num_bits);
}

void NewPPLinkManager::establishLink() {
	// create empty message
	auto *header = new L2HeaderLinkRequest(link_id);
	auto *payload = new LinkRequestPayload();
	// set callback s.t. the payload can be populated just-in-time.
	payload->callback = this;
	// pass to SH link manager
	((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->sendLinkRequest(header, payload);
	// update status
	this->link_status = awaiting_reply;

	// to be able to measure the link establishment time, save the current time slot
	this->time_when_request_was_generated = mac->getCurrentSlot();
}

void NewPPLinkManager::onSlotStart(uint64_t num_slots) {
	coutd << *mac << "::" << *this << "::onSlotStart(" << num_slots << ") -> ";		
}

void NewPPLinkManager::onSlotEnd() {
	coutd << *mac << "::" << *this << "::onSlotEnd -> ";
	// update the outgoing traffic estimate if it hasn't been
	if (!outgoing_traffic_estimate.hasBeenUpdated())
		outgoing_traffic_estimate.put(0);
	// mark the outgoing traffic estimate as unset
	outgoing_traffic_estimate.reset();
}

void NewPPLinkManager::populateLinkRequest(L2HeaderLinkRequest*& header, LinkRequestPayload*& payload) {
	coutd << "populating link request -> ";

	// determine number of slots in-between transmission bursts
	burst_offset = getBurstOffset();
	// determine number of TX and RX slots
	std::pair<unsigned int, unsigned int> tx_rx_split = this->getTxRxSplit(getRequiredTxSlots(), getRequiredRxSlots(), burst_offset);
	unsigned int burst_length_tx = tx_rx_split.first,
				 burst_length_rx = tx_rx_split.second,
				 burst_length = burst_length_tx + burst_length_rx;
	// select proposal resources
	auto proposal_resources = this->slotSelection(this->proposal_num_frequency_channels, this->proposal_num_time_slots, burst_length, burst_length_tx);
	// lock them
	auto locked_resources = LockMap();
	unsigned int reply_offset = 0;
	for (const auto pair : proposal_resources) {
		const auto *frequency_channel = pair.first;
		const auto &time_slots = pair.second;
		// for PP channels, all bursts until link expiry must be locked
		if (frequency_channel->isPP())
			locked_resources.merge(this->lock_bursts(time_slots, burst_length, burst_length_tx, this->timeout_before_link_expiry, true, reservation_manager->getReservationTable(frequency_channel)));
		// for the SH, just a single slot for the reply must be reserved
		else {			
			if (time_slots.size() != 1)
				throw std::runtime_error("PPLinkManager::populateLinkRequest not 1 reply slot but " + std::to_string(time_slots.size()));
			reply_offset = time_slots.at(0);			
			auto *sh_table = reservation_manager->getBroadcastReservationTable();			
			// remember which one
			locked_resources.locks_local.push_back({sh_table, reply_offset});
			// mark as reception
			Reservation reply_reservation = Reservation(link_id, Reservation::RX);
			sh_table->mark(reply_offset, reply_reservation);			
			// and schedule a receiver
			for (auto *rx_table : reservation_manager->getRxTables()) {
				if (rx_table->isIdle(reply_offset)) {
					locked_resources.locks_receiver.push_back({rx_table, reply_offset});
					rx_table->mark(reply_offset, reply_reservation);
					break;
				}
			}						
		}
	}
	
	// populate message
	header->timeout = this->timeout_before_link_expiry;
	header->burst_length = burst_length;
	header->burst_length_tx = burst_length_tx;
	header->burst_offset = burst_offset;
	header->reply_offset = reply_offset;
	payload->proposed_resources = proposal_resources;
	coutd << "request populated -> ";
}

std::map<const FrequencyChannel*, std::vector<unsigned int>> NewPPLinkManager::slotSelection(unsigned int num_channels, unsigned int num_time_slots, unsigned int burst_length, unsigned int burst_length_tx) const {
	coutd << "slot selection -> ";
	auto proposals = std::map<const FrequencyChannel*, std::vector<unsigned int>>();
	// choose a reply slot
	const ReservationTable *sh_table = reservation_manager->getBroadcastReservationTable();
	unsigned int reply_slot = 0;
	for (int t = this->min_offset_to_allow_processing; t < sh_table->getPlanningHorizon(); t++) {
		if (sh_table->getReservation(t).isIdle()) {
			reply_slot = t;
			break;
		}
	}
	if (reply_slot == 0)
		throw std::runtime_error("PP slot selection couldn't determine a suitable slot for a link reply.");
	proposals[sh_table->getLinkedChannel()].push_back(reply_slot);
	// get reservation tables sorted by their numbers of idle slots
	auto tables_queue = reservation_manager->getSortedP2PReservationTables();
	// until we've considered a sufficient number of channels or have run out of channels
	size_t num_channels_considered = 0;
	while (num_channels_considered < num_channels && !tables_queue.empty()) {
		// get the next reservation table
		auto *table = tables_queue.top();
		tables_queue.pop();
		// make sure the channel's not blacklisted
		if (table->getLinkedChannel()->isBlocked())
			continue;
		// find time slots to propose
		auto candidate_slots = table->findPPCandidates(num_time_slots, reply_slot + this->min_offset_to_allow_processing, this->burst_offset, burst_length, burst_length_tx, this->timeout_before_link_expiry);
		coutd << "found " << candidate_slots.size() << " slots on " << *table->getLinkedChannel() << ": ";
		for (int32_t slot : candidate_slots)
			coutd << slot << ":" << slot + burst_length - 1 << " ";
		coutd << " -> ";
		// save them to the proposal map
		for (unsigned int slot : candidate_slots)
			proposals[table->getLinkedChannel()].push_back(slot);
		// increment number of channels that have been considered
		num_channels_considered++;
	}
	return proposals;
}

NewPPLinkManager::LockMap NewPPLinkManager::lock_bursts(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, unsigned int timeout, bool is_link_initiator, ReservationTable* table) {
	coutd << "locking: ";
	// remember locked resources locally, for the transmitter, and for the receiver
	std::set<unsigned int> locked_local, locked_tx, locked_rx;
	// check that slots can be locked
	for (unsigned int start_offset : start_slots) {
		// go over all bursts of the entire link
		for (unsigned int n_burst = 0; n_burst < timeout; n_burst++) {
			for (unsigned int t = 0; t < burst_length; t++) {
				// normalize to actual slot offset
				unsigned int slot = n_burst*burst_offset + start_offset + t;								
				// check local reservation
				if (table->canLock(slot)) 
					locked_local.emplace(slot);
				else {
					const Reservation &conflict_res = table->getReservation((int) slot);
					std::stringstream ss;
					ss << *mac << "::" << *this << "::lock_bursts cannot lock local ReservationTable for burst " << n_burst << "/" << timeout << " at t=" << slot << ", conflict with " << conflict_res << ".";
					throw std::range_error(ss.str());
				}
				// link initator transmits first during a burst
				bool is_tx = is_link_initiator ? t < burst_length_tx : t >= burst_length_tx;
				// check transmitter
				if (is_tx) {
					if (std::any_of(tx_tables.begin(), tx_tables.end(), [slot](ReservationTable* tx_table) { return tx_table->canLock(slot); })) 
						locked_tx.emplace(slot);
					else {
						Reservation conflict_res = Reservation();
						for (auto it = tx_tables.begin(); it != tx_tables.end() && conflict_res.isIdle(); it++) {
							const auto tx_table = *it;
							if (!tx_table->getReservation(slot).isIdle()) 
								conflict_res = tx_table->getReservation(slot);							
						}
						std::stringstream ss;
						ss << *mac << "::" << *this << "::lock_bursts cannot lock TX ReservationTable for burst " << n_burst << "/" << timeout << " at t=" << slot << ", conflict with " << conflict_res << ".";
						throw std::range_error(ss.str());
					}
				// check receiver
				} else {
					if (std::any_of(rx_tables.begin(), rx_tables.end(), [slot](ReservationTable* rx_table) { return rx_table->canLock(slot); }))
						locked_rx.emplace(slot);
					else {
						Reservation conflict_res = Reservation();
						for (auto it = rx_tables.begin(); it != rx_tables.end() && conflict_res.isIdle(); it++) {
							const auto rx_table = *it;
							if (!rx_table->getReservation(slot).isIdle()) {
								conflict_res = rx_table->getReservation(slot);
							}
						}
						std::stringstream ss;
						ss << *mac << "::" << *this << "::lock_bursts cannot lock RX ReservationTable for burst " << n_burst << "/" << timeout << " at t=" << slot << ", conflict with " << conflict_res << ".";
						throw std::range_error(ss.str());
					}					
				}
			}
		}
	}
	// actually lock them
	auto lock_map = LockMap();
	for (unsigned int slot : locked_local) {
		table->lock(slot);
		lock_map.locks_local.push_back({table, slot});
	}
	for (unsigned int slot : locked_tx) {
		for (auto* tx_table : tx_tables)
			if (tx_table->canLock(slot)) {
				tx_table->lock(slot);
				lock_map.locks_transmitter.push_back({tx_table, slot});
				break;
			}
	}
	for (unsigned int slot : locked_rx) {
		for (auto* rx_table : rx_tables)
			if (rx_table->canLock(slot)) {
				rx_table->lock(slot);
				lock_map.locks_receiver.push_back({rx_table, slot});
				break;
			}
	}
	coutd << locked_local.size() << " local + " << locked_rx.size() << " receiver + " << locked_tx.size() << " transmitter resources -> ";
	return lock_map;
}

std::pair<unsigned int, unsigned int> NewPPLinkManager::getTxRxSplit(unsigned int resource_req_me, unsigned int resource_req_you, unsigned int burst_offset) const {
	unsigned int burst_length = resource_req_me + resource_req_you;
	if (burst_length > burst_offset) {
		double tx_fraction = ((double) resource_req_me) / ((double) burst_length);
		resource_req_me = (unsigned int) (tx_fraction * burst_offset);
		resource_req_you = burst_offset - resource_req_me;
	}
	return {resource_req_me, resource_req_you};
}

unsigned int NewPPLinkManager::getBurstOffset() const {
	// TODO have upper layer set a delay target?
	// or some other way of choosing a burst offset?
	return 20;
}

unsigned int NewPPLinkManager::getRequiredTxSlots() const {
	if (!this->force_bidirectional_links && !mac->isThereMoreData(link_id))
		return 0;
	// bits
	unsigned int num_bits_per_burst = (unsigned int) outgoing_traffic_estimate.get();
	// bits/slot
	unsigned int datarate = mac->getCurrentDatarate();
	// slots
	return std::max(this->force_bidirectional_links ? uint(1) : uint(0), num_bits_per_burst / datarate);
}

unsigned int NewPPLinkManager::getRequiredRxSlots() const {
	return this->force_bidirectional_links ? std::max(uint(1), reported_resoure_requirement) : reported_resoure_requirement;
}