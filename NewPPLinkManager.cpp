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
	coutd << *this << "::onReception";
	communication_during_this_slot = true;
}

void NewPPLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {
	communication_during_this_slot = true;
}

L2Packet* NewPPLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	communication_during_this_slot = true;
	coutd << *this << "::onTransmission -> ";
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
	communication_during_this_slot = true;
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
	communication_during_this_slot = false;	
	updated_timeout_this_slot = false;
	expecting_first_data_tx_this_slot = false;
	// update slot counter
	if (this->link_state.reserved_resources.size() > 0)
		this->link_state.reserved_resources.onSlotStart();
	// decrement time until the expected reply
	if (this->time_slots_until_reply > 0)
		this->time_slots_until_reply--;
	// decrement time until next transmission burst
	if (link_status == awaiting_data_tx || link_status == link_established) {
		if (link_state.next_burst_in == 0)
			throw std::runtime_error("PPLinkManager attempted to decrement next_burst_in past zero.");
		this->link_state.next_burst_in--;	
		coutd << "next transmission burst start " << (link_state.next_burst_in == 0 ? "now" : "in " + std::to_string(link_state.next_burst_in) + " slots") << " -> ";		
		if (link_state.next_burst_in == 0) 			
			link_state.next_burst_in = link_state.burst_offset;		
	}	
	// should this resource establish the link?		
	if (link_status == awaiting_data_tx && current_reservation_table->getReservation(0) == Reservation(link_id, Reservation::RX))
		expecting_first_data_tx_this_slot = true; // then save this s.t. we can re-trigger link establishment if it's not established
	// re-attempt link establishment
	if (this->attempt_link_establishment_again) {
		coutd << "re-attempting link establishment -> ";
		establishLink();
		this->attempt_link_establishment_again = false;
	}
}

void NewPPLinkManager::onSlotEnd() {
	// decrement timeout
	if (current_reservation_table != nullptr && communication_during_this_slot && current_reservation_table->isBurstEnd(0, link_id)) {
		coutd << *mac << "::" << *this << "::onSlotEnd -> ";
		if (decrementTimeout())
			onTimeoutExpiry();	
	}
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
	// has an expected first data transmission not arrived?
	if (link_status == awaiting_data_tx && expecting_first_data_tx_this_slot) {
		coutd << "expected first data transmission hasn't arrived -> reply must've been lost -> trying to establish a new link -> ";
		mac->statistcReportPPLinkMissedFirstDataTx();
		cancelLink();
		establishLink();
	}
	// have we missed a transmission burst?
	if (link_status == link_established && this->link_state.next_burst_in == 0)
		throw std::runtime_error("transmission burst appears to have been missed");	
	LinkManager::onSlotEnd();
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
			locked_resources.add_scheduled_resource(sh_table, reply_offset);
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

ReservationMap NewPPLinkManager::lock_bursts(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, unsigned int timeout, bool is_link_initiator, ReservationTable* table) {
	coutd << "locking: ";
	// remember locked resources locally, for the transmitter, and for the receiver
	std::set<unsigned int> locked_local, locked_tx, locked_rx;
	// check that slots *can* be locked
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
		table->lock(slot, link_id);
		lock_map.add_locked_resource(table, slot);
	}
	for (unsigned int slot : locked_tx) {
		reservation_manager->getTxTable()->lock(slot, link_id);		
		lock_map.add_locked_resource(reservation_manager->getTxTable(), slot);
	}
	for (unsigned int slot : locked_rx) {
		for (auto* rx_table : rx_tables)
			if (rx_table->canLock(slot)) {
				table->lock(slot, link_id);
				lock_map.add_locked_resource(rx_table, slot);
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

void NewPPLinkManager::processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin) {	
	coutd << *mac << "::" << *this << "::processLinkRequestMessage -> ";	
	// check whether this user is meant by this request
	const MacId& dest_id = header->getDestId();		
	if (dest_id != mac->getMacId()) { // if not
		coutd << "third-party link request between " << origin << " and " << dest_id << " -> ";
		mac->statisticReportThirdPartyLinkRequestReceived();		
		// process it through a third part link
		ThirdPartyLink &link = mac->getThirdPartyLink(origin, dest_id);
		link.processLinkRequestMessage(header, payload);
	} else { // if we are the recipient
		coutd << "link request from " << origin << " to us -> own link status is '" << link_status << "' -> ";		
		mac->statisticReportLinkRequestReceived();
		// initial link request
		if (this->link_status == link_not_established) {
			coutd << "treating this as an initial link establishment attempt -> ";
			this->processLinkRequestMessage_initial(header, payload);
		// cancel the own request and process this one
		} else if (this->link_status == awaiting_request_generation) {		
			cancelLink();		
			coutd << "canceling own request -> " << ((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->cancelLinkRequest(link_id) << " requests cancelled -> ";
			coutd << "processing request -> ";
			this->processLinkRequestMessage_initial(header, payload);
		// cancel the own reply expectation and process this request
		} else if (this->link_status == awaiting_reply) {
			cancelLink();		
			coutd << "processing request -> ";
			this->processLinkRequestMessage_initial(header, payload);
		// cancel the scheduled link and process this request
		} else if (this->link_status == awaiting_data_tx) {
			cancelLink();
			size_t num_replies_cancelled = ((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->cancelLinkReply(link_id);
			if (num_replies_cancelled > 0)
				coutd << "cancelled " << num_replies_cancelled << " own link replies -> ";
			coutd << "processing request -> ";
			this->processLinkRequestMessage_initial(header, payload);
		// link re-establishment: cancel remaining scheduled resources and process this request
		} else if (this->link_status == link_established) {		
			cancelLink();
			if (((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->cancelLinkRequest(link_id) != 0)
				throw std::runtime_error("PPLinkManager::processLinkRequestMessage cancelled a link request while the link is established - this shouldn't have happened!");
			if (((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->cancelLinkReply(link_id))
				throw std::runtime_error("PPLinkManager::processLinkRequestMessage cancelled a link reply while the link is established - this shouldn't have happened!");
			coutd << "processing request -> ";
			this->processLinkRequestMessage_initial(header, payload);
		} else {	
			throw std::runtime_error("unexpected link status during NewPPLinkManager::processLinkRequestMessage: " + std::to_string(this->link_status));	
		}	
	}
} 

void NewPPLinkManager::processLinkRequestMessage_initial(const L2HeaderLinkRequest*& request_header, const LinkManager::LinkEstablishmentPayload*& payload) {
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
			// here, the header field's burst_length_tx must be used, during which a receiver must be available
			auto chosen_resource = chooseRandomResource(resources, request_header->burst_length, request_header->burst_length_tx);
			const FrequencyChannel *selected_freq_channel = chosen_resource.first;
			unsigned int first_burst_in = chosen_resource.second;
			// save the link state
			bool is_link_initiator = false; // we've *received* a request
			// compute the local burst_length_tx
			unsigned int my_burst_length_tx = request_header->burst_length - request_header->burst_length_tx;
			this->link_state = LinkState(this->timeout_before_link_expiry, request_header->burst_offset, request_header->burst_length, my_burst_length_tx, first_burst_in, is_link_initiator, selected_freq_channel);				
			coutd << "randomly chose " << first_burst_in << "@" << *selected_freq_channel << " -> ";
			// schedule the link reply
			L2HeaderLinkReply *reply_header = new L2HeaderLinkReply();
			reply_header->burst_length = link_state.burst_length;
			reply_header->burst_length_tx = link_state.burst_length_tx;
			reply_header->burst_offset = link_state.next_burst_in;
			reply_header->timeout = link_state.timeout;
			reply_header->dest_id = link_id;
			LinkEstablishmentPayload *payload = new LinkEstablishmentPayload();
			// write selected resource into payload
			payload->resources[selected_freq_channel].push_back(first_burst_in);
			sh_manager->sendLinkReply(reply_header, payload, reply_time_slot_offset);			
			// schedule resources
			this->link_state.reserved_resources = schedule_bursts(selected_freq_channel, link_state.timeout, first_burst_in, link_state.burst_length, request_header->burst_length_tx, request_header->burst_length - request_header->burst_length_tx, is_link_initiator);	
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

void NewPPLinkManager::processLinkReplyMessage(const L2HeaderLinkEstablishmentReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {
	coutd << *this << " processing link reply -> ";	
	// check whether this user is meant by this request
	const MacId& dest_id = header->getDestId();		
	if (dest_id != mac->getMacId()) { // if not
		coutd << "third-party link reply between " << origin_id << " and " << dest_id << " -> ";
		mac->statisticReportThirdPartyLinkReplyReceived();		
		// process it through a third part link
		ThirdPartyLink &link = mac->getThirdPartyLink(origin_id, dest_id);
		link.processLinkReplyMessage(header, payload);
	} else { // if we are the recipient
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
		this->link_state.reserved_resources.unlock(link_id);
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
}

ReservationMap NewPPLinkManager::schedule_bursts(const FrequencyChannel *channel, const unsigned int timeout, const unsigned int first_burst_in, const unsigned int burst_length, const unsigned int burst_length_tx, const unsigned int burst_length_rx, bool is_link_initiator) {		
	ReservationMap reservation_map;
	this->assign(channel);		
	Reservation::Action action_1 = is_link_initiator ? Reservation::TX : Reservation::RX,
						action_2 = is_link_initiator ? Reservation::RX : Reservation::TX;
	// go over each transmission burst
	for (int burst = 0; burst < timeout; burst++) {
		// normalize to burst start
		int burst_start_offset = first_burst_in + burst*link_state.burst_offset;
		// go over link initiator's TX slots
		for (int i = 0; i < burst_length_tx; i++) {
			int slot_offset = burst_start_offset + i;
			// make sure that the reservation is idle locally
			if (!this->current_reservation_table->getReservation(slot_offset).isIdle()) {				
				for (size_t t = 0; t < 25; t++)
					std::cout << "t=" << t << ": " << this->current_reservation_table->getReservation(slot_offset + t) << std::endl;
				std::stringstream s;
				s << *mac << "::" << *this << "::processLinkReply couldn't schedule a " << action_1 << " resource in " << slot_offset << " slots. It is " << this->current_reservation_table->getReservation(slot_offset) << ", when it should be idle.";
				throw std::runtime_error(s.str());
			}
			// make sure that hardware is available
			if (action_1 == Reservation::TX) {
				const auto &tx_reservation = reservation_manager->getTxTable()->getReservation(slot_offset);
				bool transmitter_available = tx_reservation.isIdle();
				if (!transmitter_available) {
					std::stringstream s;
					s << *mac << "::" << *this << "::processLinkReply couldn't schedule a " << action_1 << " resource in " << slot_offset << " slots. The transmitter will be busy with " << tx_reservation << ", when it should be idle.";
					throw std::runtime_error(s.str());
				}
			} else {
				bool receiver_available = std::any_of(reservation_manager->getRxTables().begin(), reservation_manager->getRxTables().end(), [slot_offset](const ReservationTable *rx_table){return rx_table->getReservation(slot_offset).isIdle();});
				if (!receiver_available) {
					std::stringstream s;
					s << *mac << "::" << *this << "::processLinkReply couldn't schedule a " << action_1 << " resource in " << slot_offset << " slots. The receivers will be busy with ";
					for (const auto *rx_table : reservation_manager->getRxTables())
						s << rx_table->getReservation(slot_offset) << " ";
					s << "when at least one should be idle.";
					throw std::runtime_error(s.str());
				}
			}
			this->current_reservation_table->mark(slot_offset, Reservation(link_id, action_1));						
			reservation_map.add_scheduled_resource(this->current_reservation_table, slot_offset);
		}
		// go over the link initator's RX slots
		for (int i = 0; i < burst_length_rx; i++) {
			int slot_offset = burst_start_offset + burst_length_tx + i;
			// make sure that the reservation is idle locally
			if (!this->current_reservation_table->getReservation(slot_offset).isIdle()) {
				std::stringstream s;
				s << *mac << "::" << *this << "::processLinkReply couldn't schedule a " << action_2 << " resource in " << slot_offset << " slots. It is " << this->current_reservation_table->getReservation(slot_offset) << ", when it should be idle.";
				throw std::runtime_error(s.str());
			}
			// make sure that hardware is available
			if (action_2 == Reservation::TX) {
				const auto &tx_reservation = reservation_manager->getTxTable()->getReservation(slot_offset);
				bool transmitter_available = tx_reservation.isIdle();
				if (!transmitter_available) {
					std::stringstream s;
					s << *mac << "::" << *this << "::processLinkReply couldn't schedule a " << action_2 << " resource in " << slot_offset << " slots. The transmitter will be busy with " << tx_reservation << ", when it should be idle.";
					throw std::runtime_error(s.str());
				}
			} else {
				bool receiver_available = std::any_of(reservation_manager->getRxTables().begin(), reservation_manager->getRxTables().end(), [slot_offset](const ReservationTable *rx_table){return rx_table->getReservation(slot_offset).isIdle();});
				if (!receiver_available) {
					std::stringstream s;
					s << *mac << "::" << *this << "::processLinkReply couldn't schedule a " << action_2 << " resource in " << slot_offset << " slots. The receivers will be busy with ";
					for (const auto *rx_table : reservation_manager->getRxTables())
						s << rx_table->getReservation(slot_offset) << " ";
					s << "when at least one should be idle.";
					throw std::runtime_error(s.str());
				}
			}
			this->current_reservation_table->mark(slot_offset, Reservation(link_id, action_2));		
			reservation_map.add_scheduled_resource(this->current_reservation_table, slot_offset);
		}
	}	
	return reservation_map;
}

void NewPPLinkManager::cancelLink() {
	coutd << "cancelling link -> ";
	if (link_status != LinkManager::link_not_established) {
		if (this->link_status == LinkManager::Status::awaiting_request_generation || this->link_status == LinkManager::Status::awaiting_reply) {
			coutd << "unlocking -> ";
			this->link_state.reserved_resources.unlock(link_id);		
		} else if (this->link_status == LinkManager::Status::awaiting_data_tx || this->link_status == LinkManager::Status::link_established) {
			coutd << "unscheduling -> ";
			this->link_state.reserved_resources.unschedule();
		} else
			throw std::runtime_error("PPLinkManager::cancelLink for unexpected link_status: " + std::to_string(link_status));
		this->link_state.reserved_resources.clear();
		coutd << "unassigning frequency channel -> ";
		assign(nullptr);
		coutd << "changing link status '" << this->link_status << "->";			
		link_status = link_not_established;		
		coutd << this->link_status << "' -> ";

	} else
		coutd << "link is not established -> ";	
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
	MacId dest_id = header->dest_id;
	if (dest_id != mac->getMacId()) {
		coutd << "discarding unicast message not intended for us -> ";
		return;
	} else {
		mac->statisticReportUnicastMessageProcessed();
		
		if (link_status == awaiting_data_tx) {
			// establish link
			coutd << "this establishes the link -> link status changes '" << link_status << "->";			
			link_status = link_established;			
			coutd << link_status << "' -> ";
			mac->statisticReportPPLinkEstablished();			
			int link_establishment_time = mac->getCurrentSlot() - this->time_when_request_was_generated;
			mac->statisticReportPPLinkEstablishmentTime(link_establishment_time);
			// inform upper sublayers
			mac->notifyAboutNewLink(link_id);			
		}
	}
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

bool NewPPLinkManager::decrementTimeout() {
	// Don't decrement timeout if
	// (1) the link is not established right now
	if (link_status == LinkManager::link_not_established) {
		coutd << "link not established; not decrementing timeout -> ";
		return false;
	}
	// (2) we are in the process of initial establishment.
	if (link_status == LinkManager::awaiting_request_generation || link_status == LinkManager::awaiting_reply || link_status == LinkManager::awaiting_data_tx) {
		coutd << "link being established; not decrementing timeout -> ";
		return false;
	}
	// (3) it has already been updated this slot.
	if (updated_timeout_this_slot) {
		coutd << "already decremented timeout this slot; not decrementing timeout -> ";
		return link_state.timeout == 0;
	}	

	updated_timeout_this_slot = true;

	if (link_state.timeout == 0)
		throw std::runtime_error("PPLinkManager::decrementTimeout attempted to decrement timeout past zero.");
	coutd << "timeout " << link_state.timeout << "->";
	link_state.timeout--;
	coutd << link_state.timeout << " -> ";
	return link_state.timeout == 0;
}

void NewPPLinkManager::onTimeoutExpiry() {
	coutd << "timeout reached -> ";	
	link_state.reserved_resources.clear(); // no need to unschedule anything
	cancelLink();
	mac->statisticReportPPLinkExpired();
	// re-establish the link if there is more data
	if (mac->isThereMoreData(link_id)) 
		notifyOutgoing((unsigned long) outgoing_traffic_estimate.get());
}