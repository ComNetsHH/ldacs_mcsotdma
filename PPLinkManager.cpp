#include "PPLinkManager.hpp"
#include "SHLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "SlotCalculator.hpp"
#include <set>

using namespace TUHH_INTAIRNET_MCSOTDMA;

PPLinkManager::PPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : LinkManager(link_id, reservation_manager, mac) {}

void PPLinkManager::onReceptionReservation() {

}

L2Packet* PPLinkManager::onTransmissionReservation() {		
	return nullptr;
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
}

void PPLinkManager::establishLink() {	
	coutd << "starting link establishment -> ";	
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

}

void PPLinkManager::onSlotEnd() {

}

void PPLinkManager::processUnicastMessage(L2HeaderPP*& header, L2Packet::Payload*& payload) {

}

double PPLinkManager::getNumTxPerTimeSlot() const {
	if (!isActive())
		throw std::runtime_error("cannot call PPLinkManager::getNumSlotsUntilExpiry for inactive link");
	return 1.0 / (5.0 * std::pow(2.0, period));
}

bool PPLinkManager::isActive() const {
	return !(link_status == LinkManager::Status::link_not_established || link_status == LinkManager::Status::awaiting_request_generation);
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

void PPLinkManager::notifyLinkRequestSent(int num_bursts_forward, int num_recipient_tx, int period, int expected_link_start) {
	coutd << *this << " updating status " << link_status << " -> ";
	link_status = awaiting_reply;
	coutd << link_status << " -> ";
	this->num_initiator_tx = num_bursts_forward;
	this->num_recipient_tx = num_recipient_tx;
	this->period = period;
	this->timeout = mac->getDefaultPPLinkTimeout();
	this->next_tx_in = expected_link_start;
}

int PPLinkManager::getRemainingTimeout() const {
	return this->timeout + (link_status == awaiting_reply ? next_tx_in : 0);
}

void PPLinkManager::acceptLinkRequest(LinkProposal proposal) {
	coutd << *this << " accepting link request -> ";
	// schedule resources
	coutd << "unlocking " << reserved_resources.size() << " locked resources -> ";
	reserved_resources.unlock_either_id(mac->getMacId(), link_id);
	coutd << "scheduling resources -> ";
	channel = reservation_manager->getFreqChannelByCenterFreq(proposal.center_frequency);
	reservation_manager->scheduleBursts(channel, proposal.slot_offset, proposal.num_tx_initiator, proposal.num_tx_recipient, proposal.period, mac->getDefaultPPLinkTimeout(), link_id, mac->getMacId(), false);	
	// update status
	coutd << "status '" << link_status << "'->'";
	this->link_status = link_established;
	coutd << link_status << "' -> ";
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
		utilization.slot_offset = next_tx_in;
		utilization.timeout = timeout;
	}
	return utilization;
}