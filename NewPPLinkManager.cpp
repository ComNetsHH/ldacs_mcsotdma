//
// Created by Sebastian Lindner on 12/21/21.
//

#include "NewPPLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "SHLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

NewPPLinkManager::NewPPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : LinkManager(link_id, reservation_manager, mac) {}

void NewPPLinkManager::onReceptionBurstStart(unsigned int burst_length) {	
}

void NewPPLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {
	throw std::runtime_error("onReceptionBurst not implemented");
}

L2Packet* NewPPLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	coutd << *this << "::onTransmissionBurst -> ";			
	// instantiate packet
	auto *packet = new L2Packet();
	// add base header
	packet->addMessage(new L2HeaderBase(mac->getMacId(), link_state.burst_offset, link_state.burst_length, getRequiredTxSlots(), link_state.timeout), nullptr);
	// request payload
	size_t capacity = mac->getCurrentDatarate() - packet->getBits();
	coutd << "requesting " << capacity << " bits from upper sublayer -> ";
	auto *data = mac->requestSegment(capacity, link_id);
	// add payload
	for (size_t i = 0; i < data->getPayloads().size(); i++) 
		if (data->getHeaders().at(i)->frame_type != L2Header::base)
			packet->addMessage(data->getHeaders().at(i), data->getPayloads().at(i));	
	// return packet
	mac->statisticReportUnicastSent();
	return packet;
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
	coutd << "starting link establishment -> ";
	if (this->link_status == link_established) {
		coutd << "status is '" << this->link_status << "' -> no need to establish -> ";
		return;
	}
	// create empty message
	auto *header = new L2HeaderLinkRequest(link_id);
	auto *payload = new LinkEstablishmentPayload();
	// set callback s.t. the payload can be populated just-in-time.
	payload->callback = this;
	// pass to SH link manager
	((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->sendLinkRequest(header, payload);
	// update status
	coutd << "changing link status '" << this->link_status << "->" << awaiting_request_generation << "' -> ";
	this->link_status = awaiting_request_generation;	

	// to be able to measure the link establishment time, save the current time slot
	this->time_when_request_was_generated = mac->getCurrentSlot();
}

void NewPPLinkManager::onSlotStart(uint64_t num_slots) {
	coutd << *mac << "::" << *this << "::onSlotStart(" << num_slots << ") -> ";		
	// update slot counter
	if (this->link_state.reserved_resources.anyLocks())
		this->link_state.reserved_resources.num_slots_since_creation++;
	// decrement time until the expected reply
	if (this->time_slots_until_reply > 0)
		this->time_slots_until_reply--;
	// decrement time until next transmission burst
	if (this->link_state.next_burst_in > 0) {
		this->link_state.next_burst_in--;	
		coutd << "next transmission burst start " << (link_state.next_burst_in == 0 ? "now" : "in " + std::to_string(link_state.next_burst_in) + " slots") << " -> ";
	}
	// re-attempt link establishment
	if (this->attempt_link_establishment_again) {
		coutd << "re-attempting link establishment -> ";
		establishLink();
		this->attempt_link_establishment_again = false;
	}
}

void NewPPLinkManager::onSlotEnd() {
	coutd << *mac << "::" << *this << "::onSlotEnd -> ";
	// update the outgoing traffic estimate if it hasn't been
	if (!outgoing_traffic_estimate.hasBeenUpdated())
		outgoing_traffic_estimate.put(0);
	// mark the outgoing traffic estimate as unset
	outgoing_traffic_estimate.reset();
	// has an expected reply not arrived?
	if (link_status == awaiting_reply && this->time_slots_until_reply == 0) {
		coutd << "expected reply hasn't arrived -> trying to establish a new link -> ";
		mac->statistcReportPPLinkMissedLastReplyOpportunity();
		cancelLink();		
		establishLink();
	}
	// have we missed a transmission burst?
	if (link_status == link_established && this->link_state.next_burst_in == 0)
		throw std::runtime_error("transmission burst appears to have been missed");
}

void NewPPLinkManager::populateLinkRequest(L2HeaderLinkRequest*& header, LinkEstablishmentPayload*& payload) {
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
	if (proposal_resources.size() < size_t(2)) { // 1 proposal is the link reply, so we expect at least 2
		coutd << "couldn't determine any proposal resources -> will attempt again next slot -> ";
		this->attempt_link_establishment_again = true;
		throw std::invalid_argument("couldn't determine any proposal resources");
	}		
	// remember link parameters
	unsigned int next_burst_in = 0; // don't know what the partner will choose
	FrequencyChannel *chosen_freq_channel = nullptr; // don't know what the partner will choose
	bool is_link_initiator = true; // sender of the request is the initiator by convention	
	this->link_state = LinkState(this->timeout_before_link_expiry, burst_offset, burst_length, burst_length_tx, next_burst_in, is_link_initiator, chosen_freq_channel);
	// lock them
	auto &locked_resources = this->link_state.reserved_resources;
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
			locked_resources.resource_reservations.push_back({sh_table, reply_offset});
			// mark as reception
			Reservation reply_reservation = Reservation(link_id, Reservation::RX);
			sh_table->mark(reply_offset, reply_reservation);			
			// and schedule a receiver
			for (auto *rx_table : reservation_manager->getRxTables()) {
				if (rx_table->isIdle(reply_offset)) {					
					rx_table->mark(reply_offset, reply_reservation);
					break;
				}
			}						
		}
	}
	this->link_state.reply_offset = reply_offset;	 
	
	// populate message
	header->timeout = this->timeout_before_link_expiry;
	header->burst_length = burst_length;
	header->burst_length_tx = burst_length_tx;
	header->burst_offset = burst_offset;
	header->reply_offset = reply_offset;
	payload->resources = proposal_resources;
	coutd << "request populated -> expecting reply in " << reply_offset << " slots, changing link status '" << this->link_status << "->";
	// remember when the reply is expected
	this->time_slots_until_reply = reply_offset;
	this->link_status = awaiting_reply;	
	coutd << this->link_status << "' -> ";		
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

NewPPLinkManager::ReservationMap NewPPLinkManager::lock_bursts(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, unsigned int timeout, bool is_link_initiator, ReservationTable* table) {
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
	auto lock_map = ReservationMap();
	for (unsigned int slot : locked_local) {
		table->mark(slot, Reservation(link_id, Reservation::LOCKED));
		lock_map.resource_reservations.push_back({table, slot});
	}
	for (unsigned int slot : locked_tx) {
		for (auto* tx_table : tx_tables)
			if (tx_table->canLock(slot)) {
				table->mark(slot, Reservation(link_id, Reservation::LOCKED));				
				break;
			}
	}
	for (unsigned int slot : locked_rx) {
		for (auto* rx_table : rx_tables)
			if (rx_table->canLock(slot)) {
				table->mark(slot, Reservation(link_id, Reservation::LOCKED));				
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

void NewPPLinkManager::processLinkRequestMessage(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) {
	coutd << *mac << "::" << *this << "::processLinkRequestMessage -> own link status is '" << link_status << "' -> ";
	mac->statisticReportLinkRequestReceived();
	// initial link request
	if (this->link_status == link_not_established) {
		coutd << "treating this as an initial link establishment attempt -> ";
		this->processLinkRequestMessage_initial((const L2HeaderLinkRequest*&) header, (const LinkManager::LinkEstablishmentPayload*&) payload);
	// cancel the own request and process this one
	} else if (this->link_status == awaiting_request_generation) {		
		cancelLink();		
		coutd << "canceling own request -> " << ((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->cancelLinkRequest(link_id) << " requests cancelled -> ";
		coutd << "processing request -> ";
		this->processLinkRequestMessage_initial((const L2HeaderLinkRequest*&) header, (const LinkManager::LinkEstablishmentPayload*&) payload);
	// cancel the own reply expectation and process this request
	} else if (this->link_status == awaiting_reply) {
		throw std::runtime_error("handling link requests when own link is awaiting reply is not implemented");
	// cancel the scheduled link and process this request
	} else if (this->link_status == awaiting_data_tx) {
		throw std::runtime_error("handling link requests when own link is awaiting data transmission is not implemented");
	// link re-establishment
	} else if (this->link_status == link_established) {
		this->processLinkRequestMessage_reestablish(header, payload);
	} else {	
		throw std::runtime_error("unexpected link status during NewPPLinkManager::processLinkRequestMessage: " + std::to_string(this->link_status));	
	}
} 

void NewPPLinkManager::processLinkRequestMessage_initial(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload) {
	coutd << "checking for viable resources -> ";
	// check whether reply slot is viable	
	bool free_to_send_reply = false;	
	unsigned int reply_time_slot_offset = 0;
	auto resources = payload->resources;
	SHLinkManager *sh_manager = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
	for (auto it = resources.begin(); it != resources.end(); it++) {
		auto pair = *it;
		if (pair.first->isSH()) {
			reply_time_slot_offset = pair.second.at(0);
			free_to_send_reply = sh_manager->canSendLinkReply(reply_time_slot_offset);
			// remove this item
			resources.erase(it);
			break;
		}
	}	 		
	coutd << "reply on SH in " << reply_time_slot_offset << " slots is " << (free_to_send_reply ? "viable" : "NOT viable") << " -> ";
	if (free_to_send_reply) {		
		try {				
			// randomly choose a viable resource
			auto chosen_resource = chooseRandomResource(resources, header->burst_length, header->burst_length_tx);
			const FrequencyChannel *selected_freq_channel = chosen_resource.first;
			unsigned int first_burst_in = chosen_resource.second;
			// save the link state
			bool is_link_initiator = false; // we've *received* a request
			this->link_state = LinkState(this->timeout_before_link_expiry, header->burst_offset, header->burst_length, header->burst_length_tx, first_burst_in, is_link_initiator, selected_freq_channel);				
			coutd << "randomly chose " << first_burst_in << "@" << *selected_freq_channel << " -> ";
			// schedule the link reply
			L2HeaderLinkReply *header = new L2HeaderLinkReply();
			header->burst_length = link_state.burst_length;
			header->burst_length_tx = link_state.burst_length_tx;
			header->burst_offset = link_state.next_burst_in;
			header->timeout = link_state.timeout;
			header->dest_id = link_id;
			LinkEstablishmentPayload *payload = new LinkEstablishmentPayload();
			// write selected resource into payload
			payload->resources[selected_freq_channel].push_back(first_burst_in);
			sh_manager->sendLinkReply(header, payload, reply_time_slot_offset);			
			// schedule resources
			this->link_state.reserved_resources = schedule_bursts(selected_freq_channel, link_state.timeout, first_burst_in, link_state.burst_length, link_state.burst_length_tx, link_state.burst_length_rx, is_link_initiator);	
			coutd << "scheduled transmission bursts -> ";
			// update link status
			coutd << "updating link status '" << this->link_status << "->";
			this->link_status = LinkManager::awaiting_data_tx;
			coutd << this->link_status << "' -> ";
		} catch (const std::invalid_argument &e) {
			coutd << "no proposed resources were viable -> attempting own link establishment -> ";
			mac->statisticReportLinkRequestRejectedDueToUnacceptablePPResourceProposals();
			establishLink();
		}
	} else { // sending reply is not viable 
		coutd << "attempting own link establishment -> ";
		mac->statisticReportLinkRequestRejectedDueToUnacceptableReplySlot();		
		establishLink(); // so make a counter proposal
	}
}

std::pair<const FrequencyChannel*, unsigned int> NewPPLinkManager::chooseRandomResource(const std::map<const FrequencyChannel*, std::vector<unsigned int>>& resources, unsigned int burst_length, unsigned int burst_length_tx) {
	std::vector<const FrequencyChannel*> viable_resource_channel;
	std::vector<unsigned int> viable_resource_slot;
	// For each resource...
	coutd << "checking ";
	for (const auto &resource : resources) {
		const FrequencyChannel *channel = resource.first;
		const std::vector<unsigned int> &slots = resource.second;
		// ... get the channel's ReservationTable
		const ReservationTable *table = reservation_manager->getReservationTable(channel);
		// ... and check all proposed slot ranges, saving viable ones.		
		for (unsigned int slot : slots) {
			coutd << slot << "@" << *channel << " ";
			// ... isViableProposal checks that the range is idle and hardware available
			if (isProposalViable(table, slot, burst_length, burst_length_tx, burst_offset, this->timeout_before_link_expiry)) {
				viable_resource_channel.push_back(channel);
				viable_resource_slot.push_back(slot);
				coutd << "(viable), ";
			} else
				coutd << "(busy), ";
		}
	}
	coutd << "-> ";
	if (viable_resource_channel.empty())
		throw std::invalid_argument("No viable resources were provided.");
	else {
		auto random_index = getRandomInt(0, viable_resource_channel.size());
		return {viable_resource_channel.at(random_index), viable_resource_slot.at(random_index)};
	}
}

bool NewPPLinkManager::isProposalViable(const ReservationTable *table, unsigned int burst_start, unsigned int burst_length, unsigned int burst_length_tx, unsigned int burst_offset, unsigned int timeout) const {
	bool viable = true;
	unsigned int burst_length_rx = burst_length - burst_length_tx;
	for (unsigned int burst = 0; viable && burst < timeout; burst++) {
		int slot = (int) (burst_start + burst*burst_offset);			
		// Entire slot range must be idle && receiver during first slots && transmitter during later ones.		
		viable = viable && table->isIdle(slot, burst_length)
					&& mac->isAnyReceiverIdle(slot, burst_length_tx)
					&& mac->isTransmitterIdle(slot + burst_length_tx, burst_length_rx);
	}
	return viable;
}

void NewPPLinkManager::processLinkRequestMessage_reestablish(const L2Header*& header, const L2Packet::Payload*& payload) {
	throw std::runtime_error("handling link requests when own link is established is not implemented");	
}

void NewPPLinkManager::processLinkReplyMessage(const L2HeaderLinkEstablishmentReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {
	coutd << *this << " processing link reply -> ";	
	mac->statisticReportLinkReplyReceived();
	// parse selected communication resource
	const std::map<const FrequencyChannel*, std::vector<unsigned int>>& selected_resource_map = payload->resources;
	if (selected_resource_map.size() != size_t(1))
		throw std::invalid_argument("PPLinkManager::processLinkReplyMessage got a reply that does not contain just one selected resource, but " + std::to_string(selected_resource_map.size()));
	const auto &selected_resource = *selected_resource_map.begin();
	const FrequencyChannel *selected_freq_channel = selected_resource.first;
	if (selected_resource.second.size() != size_t(1)) 
		throw std::invalid_argument("PPLinkManager::processLinkReplyMessage got a reply that does not contain just one time slot offset, but " + std::to_string(selected_resource.second.size()));
	const unsigned int selected_time_slot_offset = selected_resource.second.at(0);			
	unsigned int &timeout = this->link_state.timeout, 
				 &burst_length = this->link_state.burst_length,
				 &burst_length_tx = this->link_state.burst_length_tx,
				 &burst_length_rx = this->link_state.burst_length_rx;	
	unsigned int first_burst_in = selected_time_slot_offset - this->link_state.reply_offset; // normalize to current time slot
	link_state.next_burst_in = first_burst_in;
	bool is_link_initiator = true;
	coutd << "partner chose resource " << first_burst_in << "@" << *selected_freq_channel << " -> ";		
	// free locked resources	
	this->link_state.reserved_resources.unlock();
	this->link_state.reserved_resources.clear();
	coutd << "free'd locked resources -> ";
	// schedule resources
	this->link_state.reserved_resources = schedule_bursts(selected_freq_channel, timeout, first_burst_in, burst_length, burst_length_tx, burst_length_rx, is_link_initiator);	
	coutd << "scheduled transmission bursts -> ";
	// update link status
	coutd << "updating link status '" << this->link_status << "->";
	this->link_status = LinkManager::awaiting_data_tx;
	coutd << this->link_status << "' -> ";
}

NewPPLinkManager::ReservationMap NewPPLinkManager::schedule_bursts(const FrequencyChannel *channel, const unsigned int timeout, const unsigned int first_burst_in, const unsigned int burst_length, const unsigned int burst_length_tx, const unsigned int burst_length_rx, bool is_link_initiator) {		
	NewPPLinkManager::ReservationMap reservation_map;
	this->assign(channel);		
	Reservation::Action action_1 = is_link_initiator ? Reservation::TX : Reservation::RX,
						action_2 = is_link_initiator ? Reservation::RX : Reservation::TX;
	for (int burst = 0; burst < timeout; burst++) {
		int slot = first_burst_in + burst*this->link_state.burst_offset;
		for (int tx = 0; tx < burst_length_tx; tx++) {
			int slot_offset = slot + tx;
			if (!this->current_reservation_table->getReservation(slot_offset).isIdle()) {				
				for (size_t t = 0; t < 25; t++)
					std::cout << "t=" << t << ": " << this->current_reservation_table->getReservation(slot_offset + t) << std::endl;
				std::stringstream s;
				s << "PPLinkManager::processLinkReply couldn't schedule a " << action_1 << " resource in " << slot_offset << " slots. It is " << this->current_reservation_table->getReservation(slot_offset) << ", when it should be locked.";
				throw std::runtime_error(s.str());
			}
			this->current_reservation_table->mark(slot_offset, Reservation(link_id, action_1));						
			reservation_map.resource_reservations.push_back({this->current_reservation_table, slot_offset});
		}
		for (int rx = 0; rx < burst_length_rx; rx++) {
			int slot_offset = slot + burst_length_tx + rx;
			if (!this->current_reservation_table->getReservation(slot_offset).isIdle()) {
				std::stringstream s;
				s << "PPLinkManager::processLinkReply couldn't schedule a " << action_2 << " resource in " << slot_offset << " slots. It is " << this->current_reservation_table->getReservation(slot_offset) << ", when it should be locked.";
				throw std::runtime_error(s.str());
			}
			this->current_reservation_table->mark(slot_offset, Reservation(link_id, action_2));		
			reservation_map.resource_reservations.push_back({this->current_reservation_table, slot_offset});
		}
	}	
	return reservation_map;
}

void NewPPLinkManager::cancelLink() {
	coutd << "cancelling link -> ";
	if (this->link_status == LinkManager::Status::awaiting_request_generation || this->link_status == LinkManager::Status::awaiting_reply) {		
		coutd << "unlocking -> ";
		this->link_state.reserved_resources.unlock();
		coutd << "changing link status '" << this->link_status << "->";			
		link_status = link_not_established;		
		coutd << this->link_status << "' -> ";
	} else if (this->link_status == LinkManager::Status::awaiting_data_tx || this->link_status == LinkManager::Status::link_established) {
		coutd << "unscheduling -> ";
		this->link_state.reserved_resources.unschedule();
		coutd << "changing link status '" << this->link_status << "->";			
		link_status = link_not_established;		
		coutd << this->link_status << "' -> ";
	} else if (this->link_status == LinkManager::Status::link_not_established) {		
		// nothing to do
		coutd << "link is not establihed -> ";
	} else 
		throw std::runtime_error("PPLinkManager::cancelLink for unexpected link status: " + std::to_string(this->link_status));
	coutd << "done -> ";
}

void NewPPLinkManager::processBaseMessage(L2HeaderBase*& header) {
	coutd << *this << "::processBaseMessage -> ";
	// communication partner can indicate its resource requirements through this header field
	this->setReportedDesiredTxSlots(header->burst_length_tx);
	// communication partner is quite obviously active
	mac->reportNeighborActivity(header->src_id);
}

void NewPPLinkManager::processUnicastMessage(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	coutd << *this << "::processUnicastMessage -> ";
}

void NewPPLinkManager::setReportedDesiredTxSlots(unsigned int value) {
	if (this->force_bidirectional_links)
		this->reported_resoure_requirement = std::max(uint(1), value);
	else
		this->reported_resoure_requirement = value;
}

void NewPPLinkManager::setForceBidirectionalLinks(bool flag) {
	this->force_bidirectional_links = flag;
}