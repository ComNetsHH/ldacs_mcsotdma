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

#include "ThirdPartyLink.hpp"
#include "SHLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "SlotCalculator.hpp"
#include "L2Header.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ThirdPartyLink::ThirdPartyLink(const MacId& id_link_initiator, const MacId& id_link_recipient, MCSOTDMA_Mac *mac) 
	: id_link_initiator(id_link_initiator), id_link_recipient(id_link_recipient), locked_resources_for_initiator(), locked_resources_for_recipient(), mac(mac) {}

ThirdPartyLink::Status ThirdPartyLink::getStatus() const {
	return this->status;
}

bool ThirdPartyLink::operator==(const ThirdPartyLink &other) {
	return id_link_initiator == other.id_link_initiator && id_link_recipient == other.id_link_recipient;
}

bool ThirdPartyLink::operator!=(const ThirdPartyLink &other) {
	return !(*this == other);
}

void ThirdPartyLink::onSlotStart(size_t num_slots) {
	// update locked resources
	for (size_t t = 0; t < num_slots; t++) {
		locked_resources_for_initiator.onSlotStart(); 
		locked_resources_for_recipient.onSlotStart();
		scheduled_resources.onSlotStart();		
	}
	// update counter towards expected link reply
	if (this->num_slots_until_expected_link_reply != UNSET) {
		if (this->num_slots_until_expected_link_reply < num_slots)
			throw std::runtime_error("ThirdPartyLink::onSlotStart attempted to decrement counter until expected link reply past zero.");
		this->num_slots_until_expected_link_reply -= num_slots;		
	}
	// update counter towards link expiry	
	if (link_expiry_offset != UNSET) {
		if (link_expiry_offset < num_slots)
			throw std::runtime_error("ThirdPartyLink attempted to decrement the counter until link expiry past zero.");
		link_expiry_offset -= num_slots;				
	}
	// update normalization offset
	if (normalization_offset != UNSET)
		normalization_offset++;
}

void ThirdPartyLink::onSlotEnd() {
	// was a link reply expected this slot?	
	if (num_slots_until_expected_link_reply == 0) {
		coutd << *mac << "::" << *this << " expected link reply hasn't arrived -> resetting -> ";
		reset();		
		mac->onThirdPartyLinkReset(this); // notify MAC, which notifies all other ThirdPartyLinks, which may schedule/lock some resources that were just unscheduled/unlocked
	}	
	// does the link terminate now?
	if (link_expiry_offset == 0) {
		coutd << *mac << "::" << *this << " terminates -> resetting -> ";
		reset();
		mac->onThirdPartyLinkReset(this); // notify MAC, which notifies all other ThirdPartyLinks, which may schedule/lock some resources that were just unscheduled/unlocked
	} 
}

void ThirdPartyLink::reset() {
	coutd << *this << " resetting -> ";
	this->status = uninitialized;
	// unlock and unschedule everything
	size_t unlocks = locked_resources_for_initiator.unlock_either_id(id_link_initiator, id_link_recipient);
	coutd << "unlocked " << unlocks << " initiator locks -> ";
	unlocks = locked_resources_for_recipient.unlock_either_id(id_link_recipient, id_link_initiator);
	coutd << "unlocked " << unlocks << " recipient locks -> ";
	unlocks = scheduled_resources.unschedule({Reservation::BUSY});
	coutd << "unscheduled " << unlocks << " resources -> ";	
	locked_resources_for_initiator.reset();
	locked_resources_for_recipient.reset();
	scheduled_resources.reset();		
	// reset counters	
	num_slots_until_expected_link_reply = UNSET;	
	link_expiry_offset = UNSET;
	normalization_offset = UNSET;	
	// reset description
	link_description = LinkDescription();
}

const MacId& ThirdPartyLink::getIdLinkInitiator() const {
	return id_link_initiator;
}

const MacId& ThirdPartyLink::getIdLinkRecipient() const {
	return id_link_recipient;
}

void ThirdPartyLink::processLinkRequestMessage(const L2HeaderSH::LinkRequest& header) {
	coutd << *this << " processing link request -> ";	
	// update status
	this->status = received_request_awaiting_reply;	
	// get expected reply slot
	const auto &neighbor_observer = mac->getNeighborObserver();
	try {
		// use recipient's next broadcast if available
		this->num_slots_until_expected_link_reply = neighbor_observer.getNextExpectedBroadcastSlotOffset(id_link_recipient);
	} catch (const std::invalid_argument &e) {
		// else use initiator's, which must be known because we've just received this user's beacon
		try {
			this->num_slots_until_expected_link_reply = neighbor_observer.getNextExpectedBroadcastSlotOffset(id_link_initiator);
		} catch (const std::exception &e) {
			throw std::runtime_error("While processing a link request, couldn't determine the next broadcast of the sender, which must be part of the beacon with the link request. Did you set advertiseNextBroadcastSlotInCurrentHeader to false? If you to simulate link establishments, this should be true (at least for those users that engage in PP comms). The error: " + std::string(e.what()));
		}
	}	
	// this->reply_offset = (int) header->reply_offset; // this one is not updated and will be used in slot offset normalization when the reply is processed
	// mark the slot as RX (collisions are handled, too)
	auto *sh_manager = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);		
	sh_manager->reportThirdPartyExpectedLinkReply(this->num_slots_until_expected_link_reply, id_link_recipient);
	// parse proposed resources

	const LinkProposal &link_proposal = header.proposed_link;		
	int timeout = mac->getDefaultPPLinkTimeout();
	this->link_description = LinkDescription(link_proposal, timeout);
	this->link_description.id_link_initiator = id_link_initiator;
	this->link_description.id_link_recipient = id_link_recipient;
	this->link_description.link_established = false;
	this->link_description.timeout = timeout;
	// lock as much as possible
	normalization_offset = 0; // request reception is the reference time
	this->lockIfPossible(this->locked_resources_for_initiator, this->locked_resources_for_recipient, link_proposal, normalization_offset, timeout);
	coutd << "locked " << locked_resources_for_initiator.size() << " initiator resources and " << locked_resources_for_recipient.size() << " recipient resources -> ";
}

void ThirdPartyLink::lockIfPossible(ReservationMap& locks_initiator, ReservationMap& locks_recipient, const LinkProposal &proposed_link, const int &normalization_offset, const int &timeout) {	
	// get subchannel
	const FrequencyChannel *channel;
	try {
		channel = mac->getReservationManager()->getFreqChannelByCenterFreq(proposed_link.center_frequency);
	} catch (const std::exception &e) {
		std::stringstream ss;
		ss << *mac << "::" << *this << " error in lockIfPossible: " << e.what();
		throw std::runtime_error(ss.str());
	}	
	// get time slots	
	auto slots = SlotCalculator::calculateAlternatingBursts(proposed_link.slot_offset - normalization_offset, proposed_link.num_tx_initiator, proposed_link.num_tx_recipient, proposed_link.period, timeout);
	ReservationTable *table = mac->getReservationManager()->getReservationTable(channel);		
	const auto &tx_slots = slots.first;
	const auto &rx_slots = slots.second;
	// for each of the link initiator's transmission slot
	for (int slot_offset : tx_slots) {
		try {										
			bool could_lock = table->lock_either_id(slot_offset, id_link_initiator, id_link_recipient);
			if (could_lock) 				
				locks_initiator.add_locked_resource(table, slot_offset);														
		} catch (const id_mismatch &e) {					
			// do nothing if it couldn't be locked
			// it may very well already be reserved/locked to another user			
		} catch (const cannot_lock &e) {
			// do nothing if it couldn't be locked
			// it may very well already be reserved/locked to another user			
		} catch (const std::exception &e) {
			std::stringstream ss;
			ss << *mac << "::" << *this << "::processLinkRequestMessage error: " << e.what();
			throw std::runtime_error(ss.str());
		}
	}			
	// for each of the link recipient's transmission slot
	for (int slot_offset : rx_slots) {									
		try {
			bool could_lock = table->lock_either_id(slot_offset, id_link_recipient, id_link_initiator);
			if (could_lock) 
				locks_recipient.add_locked_resource(table, slot_offset);						
		} catch (const id_mismatch &e) {
			// do nothing if it couldn't be locked
			// it may very well already be reserved/locked to another user					
		} catch (const cannot_lock &e) {
			// do nothing if it couldn't be locked
			// it may very well already be reserved/locked to another user			
		} catch (const std::exception &e) {
			std::stringstream ss;
			ss << *mac << "::" << *this << "::processLinkRequestMessage error: " << e.what();
			throw std::runtime_error(ss.str());
		}				
	}	
}

void ThirdPartyLink::processLinkReplyMessage(const L2HeaderSH::LinkReply& header, const MacId& origin_id) {	
	coutd << *this << " processing link reply -> ";			
	// reset	
	
	coutd << "attempting to unlock " << locked_resources_for_initiator.size() << " initator locks: ";
	size_t unlocks = locked_resources_for_initiator.unlock_either_id(id_link_initiator, id_link_recipient);
	coutd << " unlocked " << unlocks << " -> ";	
	locked_resources_for_initiator.reset();		
	
	coutd << "attempting to unlock " << locked_resources_for_recipient.size() << " recipient locks: ";
	unlocks = locked_resources_for_recipient.unlock_either_id(id_link_recipient, id_link_initiator);
	coutd << "unlocked " << unlocks << " -> ";		
	locked_resources_for_recipient.reset();	
	// update status
	this->status = received_reply_link_established;	
	// parse selected resource		
	const FrequencyChannel *selected_freq_channel = mac->getReservationManager()->getFreqChannelByCenterFreq(header.proposed_link.center_frequency);	
	const unsigned int selected_time_slot_offset = header.proposed_link.slot_offset;
	const int timeout = mac->getDefaultPPLinkTimeout();					
	int first_burst_slot_offset = (int) selected_time_slot_offset; 	
	// // normalize to current time slot
	// if (reply_offset != UNSET)
	// 	first_burst_slot_offset -= reply_offset; 	
	// else
	// 	first_burst_slot_offset -= burst_length;
	// save link info
	normalization_offset = 0; // reply reception is the reference time now
	this->link_description = LinkDescription(header.proposed_link, timeout);
	this->link_description.first_burst_slot_offset = first_burst_slot_offset;
	this->link_description.selected_channel = selected_freq_channel;	
	const MacId initiator_id = header.dest_id, &recipient_id = origin_id;	
	this->link_description.id_link_initiator = initiator_id;
	this->link_description.id_link_recipient = recipient_id;
	this->link_description.link_established = true;
	this->link_description.timeout = timeout;
	// schedule the link's resources		
	std::vector<std::pair<int, Reservation>> reservations;
	try {
		reservations = link_description.getRemainingLinkReservations();		
	} catch (const std::exception &e) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::processLinkReplyMessage couldn't schedule resources along this link: error during 'getRemainingLinkReservations': " << e.what();
		throw std::runtime_error(ss.str());
	}
	ReservationTable *table;
	try {		
		table = mac->getReservationManager()->getReservationTable(link_description.selected_channel);		
	} catch (const std::exception &e) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::processLinkReplyMessage couldn't schedule resources along this link: error getting ReservationTable: " << e.what();
		throw std::runtime_error(ss.str());
	}	
	try {		
		this->scheduled_resources = scheduleIfPossible(reservations, table);
	} catch (const std::exception &e) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::processLinkReplyMessage couldn't schedule resources along this link: error during 'scheduleIfPossible': " << e.what();
		throw std::runtime_error(ss.str());
	}
	// reset counters
	num_slots_until_expected_link_reply = UNSET;	
	// set new counter
	link_expiry_offset = first_burst_slot_offset + timeout*10*std::pow(2, header.proposed_link.period) - 5*std::pow(2, header.proposed_link.period);	
}

void ThirdPartyLink::onAnotherThirdLinkReset() {		
	if (this->status == uninitialized)
		return;	
	coutd << *this << " checking if additional resources can be locked or scheduled -> status is " << this->status << " -> ";
	switch (this->status) {	
		case received_request_awaiting_reply: {
			// attempt to add more locks
			size_t num_locks_initiator = this->locked_resources_for_initiator.size(), num_locks_recipient = this->locked_resources_for_recipient.size();
			this->lockIfPossible(this->locked_resources_for_initiator, this->locked_resources_for_recipient, link_description.link_proposal, this->normalization_offset, link_description.timeout);
			coutd << "additionally locked " << this->locked_resources_for_initiator.size() - num_locks_initiator << " link initiator resources and " << this->locked_resources_for_recipient.size() - num_locks_recipient << " link recipient resources -> ";			
			break;
		}
		case received_reply_link_established: {
			// attempt to reserve more resources
			size_t num_reservations = this->scheduled_resources.size();
			try {				
				auto reservations = link_description.getRemainingLinkReservations();
				auto *table = mac->getReservationManager()->getReservationTable(link_description.selected_channel);
				this->scheduled_resources = scheduleIfPossible(reservations, table);
			} catch (const std::exception &e) {
				std::stringstream ss;
				ss << *mac << "::" << *this << "::onAnotherThirdLinkReset couldn't schedule resources along this link: " << e.what();
				throw std::runtime_error(ss.str());
			}
			coutd << "additionally marked " << this->scheduled_resources.size() - num_reservations << " as BUSY -> ";
			break;
		}
		default: {
			std::stringstream ss;
			ss << *this << "::onAnotherThirdLinkReset for unrecognized status: " << this->status;
			throw std::runtime_error(ss.str()); 
			break;
		}		
	}		
}

std::vector<std::pair<int, Reservation>> ThirdPartyLink::LinkDescription::getRemainingLinkReservations() const {
	if (!link_established)
		throw std::runtime_error("ThirdPartyLink::LinkDescription::getRemainingLinkReservations for unestablished link.");
	if (first_burst_slot_offset == -1)
		throw std::runtime_error("ThirdPartyLink::LinkDescription::getRemainingLinkReservations for link with unset 'first_burst_slot_offset'.");
	std::pair<std::vector<int>, std::vector<int>> tx_rx_slots;
	try {		
		tx_rx_slots = SlotCalculator::calculateAlternatingBursts(first_burst_slot_offset, link_proposal.num_tx_initiator, link_proposal.num_tx_recipient, link_proposal.period, timeout);
	} catch (const std::exception& e) {
		throw std::runtime_error("error during calc: " + std::string(e.what()));
	}
	std::vector<std::pair<int, Reservation>> reservations;
	try {
		const auto &tx_slots = tx_rx_slots.first;
		const auto &rx_slots = tx_rx_slots.second;	
		for (auto tx_slot : tx_slots)
			reservations.push_back({tx_slot, Reservation(id_link_initiator, Reservation::BUSY)});
		for (auto rx_slot : rx_slots)
			reservations.push_back({rx_slot, Reservation(id_link_recipient, Reservation::BUSY)});
		} catch (const std::exception& e) {
			throw std::runtime_error("error after calc: " + std::string(e.what()));
		}
	return reservations;
}

ReservationMap ThirdPartyLink::scheduleIfPossible(const std::vector<std::pair<int, Reservation>>& reservations, ReservationTable *table) {
	ReservationMap reservation_map;
	for (const auto &pair : reservations) {
		int slot_offset = pair.first;
		if (table->isIdle(slot_offset)) {
			const Reservation &res = pair.second;
			table->mark(slot_offset, res);
			reservation_map.add_scheduled_resource(table, slot_offset);
		}		
	}
	return reservation_map;
}