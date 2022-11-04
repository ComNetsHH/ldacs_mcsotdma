#include "PPLinkManager.hpp"
#include "SHLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "SlotCalculator.hpp"
#include <set>

using namespace TUHH_INTAIRNET_MCSOTDMA;

PPLinkManager::PPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : LinkManager(link_id, reservation_manager, mac) {}

void PPLinkManager::onReceptionReservation() {
	reception_this_slot = true;
}

L2Packet* PPLinkManager::onTransmissionReservation() {			
	coutd << *this << "::onTransmission -> ";		
	// report start of TX burst to ARQ
	if (isStartOfTxBurst()) {
		reported_start_tx_burst_to_arq = true;
		mac->reportStartOfTxBurstToArq(link_id);		
	}
	// get capacity and request data packet
	size_t capacity = mac->getCurrentDatarate();
	coutd << "requesting " << capacity << " bits from upper sublayer -> ";
	L2Packet *packet = mac->requestSegment(capacity, link_id);	
	// set header fields
	auto *&header = (L2HeaderPP*&) packet->getHeaders().at(0);
	header->src_id = mac->getMacId();
	header->dest_id = link_id;
	// report statistics
	mac->statisticReportUnicastSent();	
	mac->statisticReportUnicastMacDelay(measureMacDelay());
	transmission_this_slot = true;
	// return packet
	return packet;	
}

void PPLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *mac << "::" << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	// trigger link establishment
	if (link_status == link_not_established) {
		coutd << "link not established -> triggering establishment -> ";		
		establishment_attempts = 0;
		establishLink();
	// unless it's already underway/established
	} else 
		coutd << "link status is '" << link_status << "' -> nothing to do." << std::endl;	
}

void PPLinkManager::establishLink() {	
	establishment_attempts++;
	coutd << "starting link establishment #" << establishment_attempts << " -> ";		
	if (establishment_attempts >= max_establishment_attempts) {
		coutd << "exceeded max. no of link establishment attempts, giving up -> ";
		mac->statisticReportMaxNoOfPPLinkEstablishmentAttemptsExceeded();
		cancelLink();
		establishment_attempts = 0;
		return;		
	}
	if (this->link_status == link_established) {
		coutd << "status is '" << this->link_status << "' -> no need to establish -> ";
		return;
	}	
	// send link request on SH
	((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->sendLinkRequest(link_id);	
	// update status
	coutd << "changing link status '" << this->link_status << "->" << awaiting_request_generation << "' -> ";
	this->link_status = awaiting_request_generation;	

	// to be able to measure the link establishment time, save the current time slot
	this->stat_link_establishment_start = mac->getCurrentSlot();		
}

void PPLinkManager::onSlotStart(uint64_t num_slots) {
	reserved_resources.onSlotStart();
	// reset flags
	transmission_this_slot = false;
	reception_this_slot = false;
	reported_start_tx_burst_to_arq = false;
	reported_end_tx_burst_to_arq = false;
}

void PPLinkManager::onSlotEnd() {
	if (link_status == awaiting_reply) {
		this->expected_link_request_confirmation_slot--;
		if (this->expected_link_request_confirmation_slot < 0) {
			coutd << *mac << "::" << *this << " expected link reply not received -> re-establishing -> ";
			mac->statisticReportPPLinkMissedLastReplyOpportunity();
			cancelLink();
			establishLink();
		}
	}	
	if (link_status == link_established) {
		try {
			// report end of bursts 
			if (transmission_this_slot) {				
				bool transmission_next_slot;
				try {
					transmission_next_slot = getNextTxSlot() == 1;
				} catch (const std::runtime_error &e) {
					// this error is thrown if no next TX slot is found
					transmission_next_slot = false;
				}
				if (!transmission_next_slot) {
					reported_end_tx_burst_to_arq = true;
					mac->reportEndOfTxBurstToArq(link_id);
				}
			}
			// decrement timeout
			bool timeout_expiry = decrementTimeout();
			if (timeout_expiry)
				onTimeoutExpiry();
		} catch (const std::exception &e) {
			std::stringstream ss;
			ss << *mac << "::" << *this << " error during onSlotEnd: " << e.what() << "."; 
			ss << "link_status=" << link_status << " timeout=" << timeout;
			throw std::runtime_error(ss.str());
		}
	}
}

bool PPLinkManager::decrementTimeout() {
	if (link_status == link_established) {
		try {
			bool is_exchange_end = is_link_initiator ? getNextRxSlot() == 0 : getNextTxSlot() == 0;		
			if (is_exchange_end) {
				coutd << *mac << "::" << *this << " timeout " << timeout << "->";
				timeout--;
				coutd << timeout << " -> ";
			}
		} catch (const std::exception &e) {
			std::stringstream ss;
			ss << *mac << "::" << *this << " couldn't decrement timeout with " << mac->getNumActivePPLinks() << " active PP links, this one has timeout=" << std::to_string(getRemainingTimeout()) << " and link_status=: " << link_status << ", error:" << std::string(e.what());
			throw std::runtime_error(ss.str());
		}
	}
	return timeout <= 0;
}

void PPLinkManager::onTimeoutExpiry() { 
	coutd << "timeout reached, link expires -> ";
	reserved_resources.reset();
	cancelLink();
	mac->statisticReportPPLinkExpired();
	establishment_attempts = 0;
	// re-establish the link if there is more data
	if (mac->isThereMoreData(link_id)) {
		coutd << "upper layer reports more data -> ";
		notifyOutgoing(1);
		// notifyOutgoing((unsigned long) outgoing_traffic_estimate.get());
	} else
		coutd << "no more data to send, keeping link closed -> ";
}

void PPLinkManager::processUnicastMessage(L2HeaderPP*& header, L2Packet::Payload*& payload) {
	coutd << *this << "::processing unicast -> ";
	mac->reportNeighborActivity(header->src_id);
}

double PPLinkManager::getNumTxPerTimeSlot() const {
	if (!isActive())
		throw std::runtime_error("cannot call PPLinkManager::getNumSlotsUntilExpiry for inactive link");
	double d = 1.0 / (10.0 * std::pow(2.0, period));
	if (d == std::numeric_limits<double>::infinity() || d == -std::numeric_limits<double>::infinity()) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::getNumTxPerTimeSlot=inf for period=" << period << " link_status=" << link_status;
		throw std::runtime_error(ss.str());
	}
	return d;
}

bool PPLinkManager::isActive() const {
	return link_status == LinkManager::Status::link_established;
}

void PPLinkManager::lockProposedResources(const LinkProposal& proposed_link) {	
	auto slots = SlotCalculator::calculateAlternatingBursts(proposed_link.slot_offset, proposed_link.num_tx_initiator, proposed_link.num_tx_recipient, proposed_link.period, mac->getDefaultPPLinkTimeout());
	const auto &tx_slots = slots.first;
	const auto &rx_slots = slots.second;
	ReservationTable *table = mac->getReservationManager()->getReservationTable(mac->getReservationManager()->getFreqChannelByCenterFreq(proposed_link.center_frequency));
	coutd << "locking: ";
	// remember locked resources locally, for the transmitter, and for the receiver
	std::set<unsigned int> locked_local, locked_tx, locked_rx;	
	// check that slots *can* be locked				
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
	coutd << locked_local.size() << " local + " << locked_rx.size() << " receiver + " << locked_tx.size() << " transmitter resources on f=" << proposed_link.center_frequency << " -> ";	
	reserved_resources.merge(lock_map);	
}

void PPLinkManager::notifyLinkRequestSent(int num_bursts_forward, int num_recipient_tx, int period, int expected_link_start, int expected_confirming_beacon_slot) {	
	// remove all locks and schedules
	cancelLink();
	// update status and parameters and counters
	link_status = awaiting_reply;
	coutd << *this << " updating status " << link_status << " -> ";	
	this->num_initiator_tx = num_bursts_forward;
	this->num_recipient_tx = num_recipient_tx;
	this->period = period;
	this->timeout = mac->getDefaultPPLinkTimeout();	
	this->expected_link_request_confirmation_slot = expected_confirming_beacon_slot;		
}

int PPLinkManager::getRemainingTimeout() const {
	return this->timeout;
}

void PPLinkManager::acceptLink(LinkProposal proposal, bool through_request, uint64_t generation_time) {
	coutd << *this << " accepting link -> ";
	coutd << "unlocking " << reserved_resources.size_locked() << " and unscheduling " << reserved_resources.size_scheduled() << " resources -> ";
	cancelLink();
	// schedule resources	
	this->period = proposal.period;
	mac->statisticReportPPPeriodUsed(this->period);
	coutd << "scheduling resources on f=" << proposal.center_frequency << "kHz -> ";
	channel = reservation_manager->getFreqChannelByCenterFreq(proposal.center_frequency);	
	this->is_link_initiator = !through_request; // recipient of a link request is not the initiator
	MacId initiator_id = this->is_link_initiator ? mac->getMacId() : link_id;
	MacId recipient_id = this->is_link_initiator ? link_id : mac->getMacId();			
	try {
		reserved_resources.merge(reservation_manager->scheduleBursts(channel, proposal.slot_offset, proposal.num_tx_initiator, proposal.num_tx_recipient, proposal.period, mac->getDefaultPPLinkTimeout(), initiator_id, recipient_id, is_link_initiator));						
	} catch (const std::exception &e) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::acceptLink has accepted faulty link: " << e.what();
		std::pair<std::vector<double>, std::vector<int>> pp_budgets = mac->getUsedPPDutyCycleBudget();
		size_t num_pp_links = pp_budgets.first.size();
		ss << "#active PP links is " << num_pp_links << " and used duty cycle budgets are: ";
		for (double d : pp_budgets.first)
			ss << d << " ";
		ss << "used SH budget is " << mac->getUsedSHDutyCycleBudget();
		throw std::runtime_error(ss.str());
	}
	current_reservation_table = reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(proposal.center_frequency));
	// update status
	coutd << "status is now '";
	this->link_status = link_established;
	coutd << link_status << "' -> ";
	if (through_request) {
		mac->statisticReportLinkRequestAccepted();
		// the time when link establishment was triggered is sent through the link request
		auto *sh = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
		stat_link_establishment_start = generation_time - sh->getNextBroadcastSlot();
	}
	mac->statisticReportPPLinkEstablished();	
	int link_establishment_time = mac->getCurrentSlot() - stat_link_establishment_start;			
	coutd << *mac << "::" << *this << " measuring link establishment time " << mac->getCurrentSlot() << " - " << stat_link_establishment_start << "=" << link_establishment_time << " -> ";
	mac->statisticReportPPLinkEstablishmentTime(link_establishment_time);
	// set timeout
	this->timeout = mac->getDefaultPPLinkTimeout();	
	((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->cancelLinkRequest(link_id);
	((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->cancelLinkReply(link_id);
	establishment_attempts = 0;
}

L2HeaderSH::LinkUtilizationMessage PPLinkManager::getUtilization() const {
	auto utilization = L2HeaderSH::LinkUtilizationMessage();
	if (link_status == link_established) {
		assert(channel != nullptr && "frequency channel unset in PPLinkManager");
		utilization.center_frequency = channel->getCenterFrequency();
		utilization.num_bursts_forward = num_initiator_tx;
		utilization.num_bursts_reverse = num_recipient_tx;
		utilization.period = period;
		utilization.slot_duration = slot_duration;
		if (timeout > 1) {
			try {
				utilization.slot_offset = getNextTxSlot();
			} catch (const std::runtime_error &e) {
				std::stringstream ss;
				ss << "Error during link utilization generation with timeout=" << std::to_string(getRemainingTimeout()) << " and link_status=: " << link_status << ", error:" << std::string(e.what());
				throw std::runtime_error(ss.str());
			}
		} else
			utilization.slot_offset = 0;		
		utilization.timeout = timeout;
	}
	return utilization;
}

void PPLinkManager::cancelLink() {		
	size_t num_unlocked = reserved_resources.unlock_either_id(mac->getMacId(), link_id);		
	size_t num_unscheduled = reserved_resources.unschedule({Reservation::TX, Reservation::RX});			
	link_status = link_not_established;
	reserved_resources.reset();
	current_reservation_table = nullptr;	
	auto *sh = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
	sh->cancelLinkRequest(link_id);
	sh->cancelLinkReply(link_id);
}

int PPLinkManager::getNextTxSlot() const {
	auto pair = reserved_resources.getNextTxReservation();
	if (pair.first == nullptr) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::getNextTxSlot couldn't find next transmission slot.";
		throw std::runtime_error(ss.str());
	}
	return pair.second;
}		

bool PPLinkManager::isStartOfTxBurst() const {
	bool is_start_of_tx_burst = false;
	try {
		int next_tx_slot = getNextTxSlot();		
		// does *not* check whether the last slot was also a transmission slot
		// because at the moment, only single-slot transmissions are supported
		if (next_tx_slot == 0)
			is_start_of_tx_burst = true;
	} catch (const std::runtime_error &e) {
		// couldn't find next TX slot
		is_start_of_tx_burst = false;
	}
	return is_start_of_tx_burst;
}

int PPLinkManager::getNextRxSlot() const {
	auto pair = reserved_resources.getNextRxReservation();
	if (pair.first == nullptr) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::getNextRxSlot couldn't find next reception slot.";
		throw std::runtime_error(ss.str());
	}
	return pair.second;
}

void PPLinkManager::setMaxNoPPLinkEstablishmentAttempts(int value) {
	this->max_establishment_attempts = value;
}