//
// Created by Sebastian Lindner on 12/21/21.
//

#include "PPLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "SHLinkManager.hpp"
#include "SlotCalculator.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

PPLinkManager::PPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : LinkManager(link_id, reservation_manager, mac) {}

void PPLinkManager::onReceptionReservation() {	
	coutd << *this << "::onReception";
	communication_during_this_slot = true;
}

L2Packet* PPLinkManager::onTransmissionReservation() {
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

void PPLinkManager::notifyOutgoing(unsigned long num_bits) {
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

void PPLinkManager::establishLink() {	
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

void PPLinkManager::onSlotStart(uint64_t num_slots) {
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

void PPLinkManager::onSlotEnd() {
	// decrement timeout
	if (current_reservation_table != nullptr && communication_during_this_slot && isBurstEnd()) {
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

bool PPLinkManager::isBurstEnd() const {
	// unestablished links won't end any transmission bursts
	if (link_status != link_established)
		return false;
	// if the current reservation doesn't involve either partner, then it also doesn't end a burst
	const auto &res = current_reservation_table->getReservation(0);
	if (res.getTarget() != mac->getMacId() && res.getTarget() != link_id)
		return false;	
	if (link_state.is_link_initator) {
		// for the link initiator, a burst ends after the last slot where it *receives* data
		if (res.isRx() && !current_reservation_table->getReservation(1).isRx())
			return true;
		else
			return false;
	} else {
		// for the link recipient, a burst ends after the last slot where it *transmits* data
		if (res.isTx() && !current_reservation_table->getReservation(1).isTx())
			return true;
		else
			return false;
	}
}

bool PPLinkManager::isContinuousTransmission() const {
	return link_state.burst_length == link_state.burst_offset;
}

unsigned int PPLinkManager::getBurstLength() const {
	return std::min(max_consecutive_tx_slots, getRequiredTxSlots()) + std::min(max_consecutive_tx_slots, getRequiredRxSlots());
}

void PPLinkManager::populateLinkRequest(L2HeaderLinkRequest*& header, LinkEstablishmentPayload*& payload) {
	coutd << "populating link request -> ";		
	// determine number of slots in-between transmission bursts
	coutd << "computing burst length: ";
	unsigned int burst_length = getBurstLength();
	coutd << burst_length << ", burst offset: ";	
	unsigned int num_neighbors = mac->getNeighborObserver().getNumActiveNeighbors();
	// one of these neighbors is the one we're establishing a link
	if (num_neighbors > 0)
		num_neighbors -= 1;
	setBurstOffset(computeBurstOffset(burst_length, num_neighbors, reservation_manager->getP2PFreqChannels().size()));	
	coutd << getBurstOffset() << (isContinuousTransmission() ? " (continuous transmission)" : "") << " -> ";
	// determine number of TX and RX slots
	std::pair<unsigned int, unsigned int> tx_rx_split = this->getTxRxSplit(getRequiredTxSlots(), getRequiredRxSlots(), getBurstOffset());
	unsigned int burst_length_tx = tx_rx_split.first,
				 burst_length_rx = tx_rx_split.second;
	burst_length = burst_length_tx + burst_length_rx;
	coutd << "proposing a link with a " << burst_length << "-slot burst length (" << burst_length_tx << " for us, " << burst_length_rx << " for them) -> ";
	// select proposal resources
	auto proposal_resources = this->slotSelection(this->proposal_num_frequency_channels, this->proposal_num_time_slots, burst_length, burst_length_tx, getBurstOffset());
	if (proposal_resources.size() < size_t(2)) { // 1 proposal is the link reply, so we expect at least 2
		coutd << "couldn't determine any proposal resources -> will attempt again next slot -> ";
		this->attempt_link_establishment_again = true;				
		this->couldnt_determine_resources_last_attempt = true; // next time, attempt with fewer resources
		mac->statisticReportLinkRequestCanceledDueToInsufficientResources();
		throw not_viable_error("couldn't determine any proposal resources");
	} else
		this->couldnt_determine_resources_last_attempt = false; 
	// remember link parameters
	unsigned int next_burst_in = 0; // don't know what the partner will choose
	FrequencyChannel *chosen_freq_channel = nullptr; // don't know what the partner will choose
	bool is_link_initiator = true; // sender of the request is the initiator by convention	
	this->link_state = LinkState(this->timeout_before_link_expiry, getBurstOffset(), burst_length, burst_length_tx, next_burst_in, is_link_initiator, chosen_freq_channel);
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
	header->burst_offset = getBurstOffset();
	header->reply_offset = reply_offset;
	payload->resources = proposal_resources;
	coutd << "request populated -> expecting reply in " << reply_offset << " slots, changing link status '" << this->link_status << "->";
	// remember when the reply is expected
	this->time_slots_until_reply = reply_offset;
	this->link_status = awaiting_reply;	
	coutd << this->link_status << "' -> ";		
}

std::map<const FrequencyChannel*, std::vector<unsigned int>> PPLinkManager::slotSelection(unsigned int num_channels, unsigned int num_time_slots, unsigned int burst_length, unsigned int burst_length_tx, unsigned int burst_offset) const {
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
		auto candidate_slots = table->findPPCandidates(num_time_slots, reply_slot + this->min_offset_to_allow_processing, burst_offset, burst_length, burst_length_tx, this->timeout_before_link_expiry);
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

ReservationMap PPLinkManager::lock_bursts(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, unsigned int timeout, bool is_link_initiator, ReservationTable* table) {
	coutd << "locking: ";
	// remember locked resources locally, for the transmitter, and for the receiver
	std::set<unsigned int> locked_local, locked_tx, locked_rx;	
	// check that slots *can* be locked
	unsigned int burst_length_rx = burst_length - burst_length_tx;
	for (unsigned int start_offset : start_slots) {
		auto tx_rx_slots = SlotCalculator::calculateTxRxSlots(start_offset, burst_length, burst_length_tx, burst_length_rx, getBurstOffset(), timeout);
		const auto &tx_slots = tx_rx_slots.first;
		const auto &rx_slots = tx_rx_slots.second;
		for (auto slot : tx_slots) {
			// check local reservation
			if (table->canLock(slot)) 
				locked_local.emplace(slot);
			else {
				const Reservation &conflict_res = table->getReservation((int) slot);
				std::stringstream ss;
				ss << *mac << "::" << *this << "::lock_bursts cannot lock local ReservationTable at t=" << slot << ", conflict with " << conflict_res << ".";
				throw std::range_error(ss.str());
			}
			// check transmitter
			if (reservation_manager->getTxTable()->canLock(slot))					
				locked_tx.emplace(slot);
			else {
				Reservation conflict_res = reservation_manager->getTxTable()->getReservation(slot);													
				std::stringstream ss;
				ss << *mac << "::" << *this << "::lock_bursts cannot lock TX ReservationTable at t=" << slot << ", conflict with " << conflict_res << ".";
				throw std::range_error(ss.str());
			}
		}
		for (auto slot : rx_slots) {
			// check local reservation
			if (table->canLock(slot)) 
				locked_local.emplace(slot);
			else {
				const Reservation &conflict_res = table->getReservation((int) slot);
				std::stringstream ss;
				ss << *mac << "::" << *this << "::lock_bursts cannot lock local ReservationTable at t=" << slot << ", conflict with " << conflict_res << ".";
				throw std::range_error(ss.str());
			}
			// check receiver
			if (std::any_of(reservation_manager->getRxTables().begin(), reservation_manager->getRxTables().end(), [slot](ReservationTable* rx_table) { return rx_table->canLock(slot); }))
				locked_rx.emplace(slot);
			else {
				Reservation conflict_res = Reservation();
				for (auto it = reservation_manager->getRxTables().begin(); it != reservation_manager->getRxTables().end() && conflict_res.isIdle(); it++) {
					const auto rx_table = *it;
					if (!rx_table->getReservation(slot).isIdle()) {
						conflict_res = rx_table->getReservation(slot);
					}
				}
				std::stringstream ss;
				ss << *mac << "::" << *this << "::lock_bursts cannot lock RX ReservationTable at t=" << slot << ", conflict with " << conflict_res << ".";
				throw std::range_error(ss.str());
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
		for (auto* rx_table : reservation_manager->getRxTables())
			if (rx_table->canLock(slot)) {
				table->lock(slot, link_id);
				lock_map.add_locked_resource(rx_table, slot);
				break;
			}
	}
	coutd << locked_local.size() << " local + " << locked_rx.size() << " receiver + " << locked_tx.size() << " transmitter resources -> ";
	return lock_map;
}

std::pair<unsigned int, unsigned int> PPLinkManager::getTxRxSplit(unsigned int resource_req_me, unsigned int resource_req_you, unsigned int burst_offset) const {
	if (resource_req_me > max_consecutive_tx_slots)
		resource_req_me = max_consecutive_tx_slots;
	if (resource_req_you > max_consecutive_tx_slots)
		resource_req_you = max_consecutive_tx_slots;
	unsigned int burst_length = resource_req_me + resource_req_you;
	if (burst_length > burst_offset) {
		double tx_fraction = ((double) resource_req_me) / ((double) burst_length);
		resource_req_me = (unsigned int) (tx_fraction * burst_offset);
		resource_req_you = burst_offset - resource_req_me;
	}
	return {resource_req_me, resource_req_you};
}

unsigned int PPLinkManager::getBurstOffset() const {	
	return this->default_burst_offset;
}

unsigned int PPLinkManager::computeBurstOffset(unsigned int burst_length, unsigned int num_neighbors, unsigned int num_pp_channels) {	
	if (adaptive_burst_offset) {		
		// to accommodate the no. of neighbors, this many slots should be left idle in-between bursts		
		unsigned int num_slots_inbetween_bursts = (unsigned int) std::ceil((4*num_neighbors*burst_length) / num_pp_channels); // the 4 stems from trying to allow each neighbor to keep at least 4 links open (e.g. north/west/south/east)		
		// the burst offset denotes the number of slots in-between the starting slot of two transmission bursts
		// so add the burst_length		
		return burst_length + num_slots_inbetween_bursts;
	} else
		return getBurstOffset();	
}

unsigned int PPLinkManager::getRequiredTxSlots() const {
	if (!this->force_bidirectional_links && !mac->isThereMoreData(link_id))
		return 0;
	// if the last attempt failed, try with minimum resources this time
	if (couldnt_determine_resources_last_attempt)
		return min_consecutive_tx_slots;
	// bits
	unsigned int num_bits_per_burst = (unsigned int) outgoing_traffic_estimate.get();
	// bits/slot
	unsigned int datarate = mac->getCurrentDatarate();
	// slots
	return std::max(this->force_bidirectional_links ? min_consecutive_tx_slots : uint(0), num_bits_per_burst / datarate);
}

unsigned int PPLinkManager::getRequiredRxSlots() const {
	// if the last attempt failed, try with minimum resources this time
	if (couldnt_determine_resources_last_attempt)
		return min_consecutive_tx_slots;
	return this->force_bidirectional_links ? std::max(uint(min_consecutive_tx_slots), reported_resoure_requirement) : reported_resoure_requirement;
}

void PPLinkManager::processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin) {	
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
		// remember number of slots that the communication partner requires
		coutd << "saving report that they require " << header->burst_length_tx << " TX slots (set to ";
		setReportedDesiredTxSlots(header->burst_length_tx);
		coutd << reported_resoure_requirement << ") -> ";
		mac->statisticReportLinkRequestReceived();
		// initial link request
		if (this->link_status == link_not_established) {
			coutd << "treating this as an initial link establishment attempt -> ";
			this->processLinkRequestMessage_initial(header, payload);
		// cancel the own request and process this one
		} else if (this->link_status == awaiting_request_generation) {		
			cancelLink();					
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
			coutd << "processing request -> ";
			this->processLinkRequestMessage_initial(header, payload);
		// link re-establishment: cancel remaining scheduled resources and process this request
		} else if (this->link_status == link_established) {		
			cancelLink();			
			coutd << "processing request -> ";
			this->processLinkRequestMessage_initial(header, payload);
		} else {	
			throw std::runtime_error("unexpected link status during PPLinkManager::processLinkRequestMessage: " + std::to_string(this->link_status));	
		}	
	}
} 

void PPLinkManager::processLinkRequestMessage_initial(const L2HeaderLinkRequest*& request_header, const LinkManager::LinkEstablishmentPayload*& payload) {		
	// check whether proposed link is sufficient for our own communication needs
	unsigned int num_tx_slots = request_header->burst_length - request_header->burst_length_tx;
	if (num_tx_slots == 0) {
		if (force_bidirectional_links || mac->isThereMoreData(link_id)) {
			// communication partner proposed zero transmission slots for us
			// but we require some -> decline, start own establishment
			coutd << "communication partner proposed zero transmission slots, but we need some -> ";
			mac->statisticReportLinkRequestRejectedDueInsufficientTXSlots();
			cancelLink();						
			establishLink();
			return;
		}
	}
	coutd << "checking for viable reply slot -> ";
	// check whether reply slot is viable	
	bool free_to_send_reply = false;	
	unsigned int reply_time_slot_offset = 0;
	auto resources = payload->resources;
	SHLinkManager *sh_manager = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
	for (auto it = resources.begin(); it != resources.end();) {
		auto pair = *it;
		if (pair.first->isSH()) {
			if (reply_time_slot_offset != 0)
				throw std::invalid_argument("PPLinkManager::processLinkRequestMessage_initial for >1 proposed resources on SH. It should just be the link reply offset.");
			reply_time_slot_offset = pair.second.at(0);
			free_to_send_reply = sh_manager->canSendLinkReply(reply_time_slot_offset);
			// remove SH resource
			it = resources.erase(it);			
			break;
		} else
			it++;
	}	 		
	coutd << "reply on SH in " << reply_time_slot_offset << " slots is " << (free_to_send_reply ? "viable" : "NOT viable") << " -> ";
	if (free_to_send_reply) {		
		try {				
			coutd << "choosing a viable, proposed resource -> ";
			// randomly choose a viable resource
			// here, the header field's burst_length_tx must be used, during which a receiver must be available
			auto chosen_resource = chooseRandomResource(resources, request_header->burst_length, request_header->burst_length_tx, request_header->burst_offset);
			const FrequencyChannel *selected_freq_channel = chosen_resource.first;
			unsigned int first_burst_in = chosen_resource.second;
			// save the link state
			bool is_link_initiator = false; // we've *received* a request
			// compute the local burst_length_tx
			unsigned int my_burst_length_tx = request_header->burst_length - request_header->burst_length_tx;
			this->link_state = LinkState(this->timeout_before_link_expiry, request_header->burst_offset, request_header->burst_length, my_burst_length_tx, first_burst_in, is_link_initiator, selected_freq_channel);				
			setBurstOffset(request_header->burst_offset);
			coutd << "randomly chose " << first_burst_in << "@" << *selected_freq_channel << " -> ";
			// schedule the link reply
			L2HeaderLinkReply *reply_header = new L2HeaderLinkReply();
			reply_header->burst_length = link_state.burst_length;
			reply_header->burst_length_tx = getRequiredTxSlots();
			reply_header->burst_offset = getBurstOffset();
			reply_header->timeout = link_state.timeout;
			reply_header->dest_id = link_id;
			LinkEstablishmentPayload *payload = new LinkEstablishmentPayload();
			// write selected resource into payload
			payload->resources[selected_freq_channel].push_back(first_burst_in);
			sh_manager->sendLinkReply(reply_header, payload, reply_time_slot_offset);			
			// schedule resources
			this->link_state.reserved_resources = scheduleBursts(selected_freq_channel, link_state.timeout, first_burst_in, link_state.burst_length, request_header->burst_length_tx, request_header->burst_length - request_header->burst_length_tx, link_state.burst_offset, is_link_initiator);	
			coutd << "scheduled transmission bursts, first_burst_in=" << first_burst_in << " burst_length=" << link_state.burst_length << " burst_length_tx=" << request_header->burst_length_tx << " burst_length_rx=" << request_header->burst_length - request_header->burst_length_tx << " burst_offset=" << link_state.burst_offset << " timeout=" << link_state.timeout << (isContinuousTransmission() ? " (continuous transmission) " : " ") << "-> ";
			// update link status
			coutd << "updating link status '" << this->link_status << "->";
			this->link_status = LinkManager::awaiting_data_tx;
			coutd << this->link_status << "' -> ";
		} catch (const not_viable_error &e) {
			coutd << "no proposed resources were viable -> attempting own link establishment -> timeout=" << link_state.timeout;
			// link_state is not set when this error is thrown
			// but to keep in timeout-sync, reset it now 
			link_state.timeout = timeout_before_link_expiry;
			mac->statisticReportLinkRequestRejectedDueToUnacceptablePPResourceProposals();
			establishLink();
		} catch (const std::exception &e) {
			std::cerr << "error during request processing: " << e.what() << std::endl;
			throw("error during request processing: " + std::string(e.what()));
		}
	} else { // sending reply is not viable 
		coutd << "attempting own link establishment -> ";
		mac->statisticReportLinkRequestRejectedDueToUnacceptableReplySlot();		
		establishLink(); // so make a counter proposal
	}
}

std::pair<const FrequencyChannel*, unsigned int> PPLinkManager::chooseRandomResource(const std::map<const FrequencyChannel*, std::vector<unsigned int>>& resources, unsigned int burst_length, unsigned int burst_length_tx, unsigned int burst_offset) {
	std::vector<const FrequencyChannel*> viable_resource_channel;
	std::vector<unsigned int> viable_resource_slot;
	// For each resource...
	coutd << "burst_length=" << burst_length << " burst_length_tx=" << burst_length_tx << " burst_offset=" << burst_offset << " -> ";
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
	if (viable_resource_channel.empty()) {		
		throw not_viable_error("No viable resources were provided.");
	} else {
		auto random_index = getRandomInt(0, viable_resource_channel.size());
		return {viable_resource_channel.at(random_index), viable_resource_slot.at(random_index)};
	}
}

bool PPLinkManager::isProposalViable(const ReservationTable *table, unsigned int burst_start, unsigned int burst_length, unsigned int burst_length_tx, unsigned int burst_offset, unsigned int timeout) const {
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

void PPLinkManager::processLinkReplyMessage(const L2HeaderLinkReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {
	coutd << *this << " processing link reply -> ";	
	// check whether this user is meant by this request
	const MacId& dest_id = header->getDestId();		
	// if not
	if (dest_id != mac->getMacId()) { 
		coutd << "third-party link reply between " << origin_id << " and " << dest_id << " -> ";
		mac->statisticReportThirdPartyLinkReplyReceived();		
		// process it through a third part link
		ThirdPartyLink &link = mac->getThirdPartyLink(origin_id, dest_id);
		link.processLinkReplyMessage(header, payload, origin_id);
	// if we are the recipient
	} else { 
		mac->statisticReportLinkReplyReceived();
		// save reported, required TX slots
		setReportedDesiredTxSlots(header->burst_length_tx);
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
					&burst_length_rx = this->link_state.burst_length_rx,
					&burst_offset = this->link_state.burst_offset;
		unsigned int first_burst_in = selected_time_slot_offset - this->link_state.reply_offset; // normalize to current time slot
		link_state.next_burst_in = first_burst_in;
		bool is_link_initiator = true;
		coutd << "partner chose resource " << first_burst_in << "@" << *selected_freq_channel << " -> ";				
		// free locked resources	
		this->link_state.reserved_resources.unlock(link_id);
		this->link_state.reserved_resources.reset();
		coutd << "free'd locked resources -> ";	
		// schedule resources
		this->link_state.reserved_resources = scheduleBursts(selected_freq_channel, timeout, first_burst_in, burst_length, burst_length_tx, burst_length_rx, burst_offset, is_link_initiator);	
		coutd << "scheduled transmission bursts (burst_length=" << burst_length << ", burst_length_tx=" << burst_length_tx << ", burst_length_rx=" << burst_length_rx << ", burst_offset=" << burst_offset << ") -> ";
		// update link status
		coutd << "updating link status '" << this->link_status << "->";
		this->link_status = LinkManager::awaiting_data_tx;
		coutd << this->link_status << "' -> ";
	}
}

ReservationMap PPLinkManager::scheduleBursts(const FrequencyChannel *channel, const unsigned int timeout, const unsigned int first_burst_in, const unsigned int burst_length, const unsigned int burst_length_tx, const unsigned int burst_length_rx, const unsigned int burst_offset, bool is_link_initiator) {			
	this->assign(channel);		
	return reservation_manager->scheduleBursts(channel, timeout, first_burst_in, burst_length, burst_length_tx, burst_length_rx, burst_offset, mac->getMacId(), link_id, is_link_initiator);
}

void PPLinkManager::cancelLink() {
	coutd << "cancelling link -> ";
	if (link_status != LinkManager::link_not_established) {
		if (this->link_status == LinkManager::Status::awaiting_request_generation || this->link_status == LinkManager::Status::awaiting_reply) {
			coutd << "unlocking -> ";
			this->link_state.reserved_resources.unlock(link_id);		
		} else if (this->link_status == LinkManager::Status::awaiting_data_tx || this->link_status == LinkManager::Status::link_established) {			
			coutd << "unscheduling " << this->link_state.reserved_resources.unschedule({Reservation::TX, Reservation::RX}) << " reservations -> ";						
		} else
			throw std::runtime_error("PPLinkManager::cancelLink for unexpected link_status: " + std::to_string(link_status));
		this->link_state.reserved_resources.reset();
		coutd << "unassigning frequency channel -> ";
		assign(nullptr);
		coutd << "changing link status '" << this->link_status << "->";			
		link_status = link_not_established;		
		coutd << this->link_status << "' -> ";
		// reset counter and flag
		no_of_consecutive_empty_bursts = 0;
		received_data_this_burst = false;
		// cancel requests and replies
		SHLinkManager *sh_link_manager = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
		size_t num_cancelled_requests = sh_link_manager->cancelLinkRequest(link_id);
		if (num_cancelled_requests > 0)
			coutd << "cancelled " << num_cancelled_requests << " pending link requests -> ";
		size_t num_cancelled_replies = sh_link_manager->cancelLinkReply(link_id);
		if (num_cancelled_replies > 0)
			coutd << "cancelled " << num_cancelled_replies << " pending link replies -> ";
	} else
		coutd << "link is not established -> ";	
	coutd << "done -> ";
}

void PPLinkManager::processBaseMessage(L2HeaderBase*& header) {
	coutd << *this << "::processBaseMessage -> ";
	// communication partner can indicate its resource requirements through this header field
	this->setReportedDesiredTxSlots(header->burst_length_tx);	
}

void PPLinkManager::processUnicastMessage(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
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
			// reset counter
			no_of_consecutive_empty_bursts = 0;						
		}

		// no else if! Should also be set if the above if has just established the link.
		if (link_status == link_established)
			received_data_this_burst = true;
	}
}

void PPLinkManager::setReportedDesiredTxSlots(unsigned int value) {
	if (this->force_bidirectional_links)
		this->reported_resoure_requirement = std::max(uint(1), value);
	else
		this->reported_resoure_requirement = value;
}

void PPLinkManager::setForceBidirectionalLinks(bool flag) {
	if (!flag)
		throw std::invalid_argument("Unidirectional links are currently not supported.");
	this->force_bidirectional_links = flag;
}

bool PPLinkManager::decrementTimeout() {
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

	// check if we should've received data but haven't
	if (isLinkEstablishedAndBidirectional() && !received_data_this_burst) {
		no_of_consecutive_empty_bursts++;
		if (no_of_consecutive_empty_bursts >= max_no_of_tolerable_empty_bursts) {
			// cancels the link
			onFaultyLink();		
			// don't attempt to decrement the timeout of the cancelled link	
			// also don't treat this as a timeout expiry, which would update the wrong statistics
			return false;
		}
	}
	// a burst has passed, so reset the flag
	received_data_this_burst = false;

	if (link_state.timeout == 0)
		throw std::runtime_error("PPLinkManager::decrementTimeout attempted to decrement timeout past zero.");
	coutd << "timeout " << link_state.timeout << "->";
	link_state.timeout--;
	coutd << link_state.timeout << " -> ";
	return link_state.timeout == 0;
}

void PPLinkManager::onFaultyLink() {
	mac->statisticReportLinkClosedEarly();
	cancelLink();
}

void PPLinkManager::onTimeoutExpiry() {
	coutd << "timeout reached -> ";	
	link_state.reserved_resources.reset(); // no need to unschedule anything
	cancelLink();
	mac->statisticReportPPLinkExpired();
	// re-establish the link if there is more data
	if (mac->isThereMoreData(link_id)) {
		coutd << "upper layer reports more data -> ";
		notifyOutgoing((unsigned long) outgoing_traffic_estimate.get());
	} else
		coutd << "no more data to send, keeping link closed -> ";
}

bool PPLinkManager::isLinkEstablishedAndBidirectional() const {
	return link_status == link_established && link_state.burst_length_tx < link_state.burst_length;
}

unsigned int PPLinkManager::getNumUtilizedResources() const {
	if (link_status == link_established)
		return link_state.burst_length;
	else
		return 0;
}

void PPLinkManager::scheduledLinkReplyCouldNotHaveBeenSent() {
	coutd << "link reply couldn't have been sent -> ";
	cancelLink();
	establishLink();
}

void PPLinkManager::setBurstOffset(unsigned int value) {
	this->default_burst_offset = value;
}

void PPLinkManager::setBurstOffsetAdaptive(bool value) {
	this->adaptive_burst_offset = value;
}

std::pair<std::vector<int>, std::vector<int>> PPLinkManager::getReservations() const {	
	if (link_status != link_established)
		throw std::runtime_error("PPLinkManager::getReservations for link status '" + std::to_string(link_status) + "'.");
	if (current_reservation_table == nullptr)
		throw std::runtime_error("PPLinkManager::getReservations for unset ReservationTable.");
	auto tx_rx_slots = std::pair<std::vector<int>, std::vector<int>>();
	for (int t = 0; t < link_state.timeout*(link_state.burst_length+link_state.burst_offset); t++) {
		if (current_reservation_table->getReservation(t) == Reservation(link_id, Reservation::TX))
			tx_rx_slots.first.push_back(t);
		else if (current_reservation_table->getReservation(t) == Reservation(link_id, Reservation::TX))
			tx_rx_slots.second.push_back(t);
	}	
	return tx_rx_slots;
}