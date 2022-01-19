//
// Created by Sebastian Lindner on 01/12/22.
//

#include "ThirdPartyLink.hpp"
#include "SHLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ThirdPartyLink::ThirdPartyLink(const MacId& id_link_initiator, const MacId& id_link_recipient, MCSOTDMA_Mac *mac) 
	: id_link_initiator(id_link_initiator), id_link_recipient(id_link_recipient), locked_resources_for_initiator(), locked_resources_for_recipient(), mac(mac) {}

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
			throw std::runtime_error("ThidPartyLink::onSlotStart attempted to decrement counter until expected link reply past zero.");
		this->num_slots_until_expected_link_reply -= num_slots;		
	}
	// update counter towards link expiry	
	if (link_expiry_offset != UNSET) {
		if (link_expiry_offset < num_slots)
			throw std::runtime_error("ThidPartyLink attempted to decrement the counter until link expiry past zero.");
		link_expiry_offset -= num_slots;				
	}
}

void ThirdPartyLink::onSlotEnd() {
	// was a link reply expected this slot?
	if (num_slots_until_expected_link_reply == 0) {
		coutd << *mac << "::" << *this << " expected link reply hasn't arrived -> resetting -> ";
		reset();
	}	
	// does the link terminate now?
	if (link_expiry_offset == 0) {
		coutd << *this << " terminates -> resetting -> ";
		reset();
	} else if (link_expiry_offset != UNSET) {
		coutd << *mac << "::" << *this << " #slots until expiry: " << link_expiry_offset << std::endl;
	}
}

void ThirdPartyLink::reset() {
	coutd << *this << " resetting -> ";
	// unlock and unschedule everything
	coutd << "unlocked " << locked_resources_for_initiator.unlock_either_id(id_link_initiator, id_link_recipient) << " initiator locks -> ";
	coutd << "unlocked " << locked_resources_for_recipient.unlock_either_id(id_link_recipient, id_link_initiator) << " recipient locks -> ";
	coutd << "unscheduled " << scheduled_resources.unschedule({Reservation::BUSY}) << " resources -> ";	
	locked_resources_for_initiator.reset();
	locked_resources_for_recipient.reset();
	scheduled_resources.reset();		
	// reset counters	
	num_slots_until_expected_link_reply = UNSET;
	reply_offset = UNSET;
	link_expiry_offset = UNSET;
}

const MacId& ThirdPartyLink::getIdLinkInitiator() const {
	return id_link_initiator;
}

const MacId& ThirdPartyLink::getIdLinkRecipient() const {
	return id_link_recipient;
}

void ThirdPartyLink::processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload) {
	coutd << *this << " processing link request -> ";
	// if anything has been locked or scheduled
	// then a new link request erases all of that
	reset();
	// parse expected reply slot
	this->num_slots_until_expected_link_reply = (int) header->reply_offset; // this one is updated each slot
	this->reply_offset = (int) header->reply_offset; // this one is not updated and will be used in slot offset normalization when the reply is processed
	// check for a potential collision with our own broadcast
	auto *sh_manager = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
	if (sh_manager->isNextBroadcastScheduled()) {
		if (sh_manager->getNextBroadcastSlot() == num_slots_until_expected_link_reply) {
			coutd << "detected collision of advertised reply slot and our own broadcast -> ";
			// re-schedule own broadcast and mark the slot as BUSY
			sh_manager->reportCollisionWithScheduledBroadcast(id_link_recipient);
		}
	}
	// now mark the slot as RX
	sh_manager->reportExpectedLinkReply(this->num_slots_until_expected_link_reply, id_link_recipient);
	// parse proposed resources	
	const std::map<const FrequencyChannel*, std::vector<unsigned int>> &proposed_resources = payload->resources;
	const unsigned int  &timeout = header->timeout, 
						&burst_length = header->burst_length, 
						&burst_length_tx = header->burst_length_tx,
						burst_length_rx = burst_length - burst_length_tx,
						&burst_offset = header->burst_offset;							
	// lock all links	
	for (const auto &pair : proposed_resources) {
		// for this subchannel
		const FrequencyChannel *channel = pair.first;		
		// skip the SH, as we have already reserved the expected link reply resource
		if (channel->isSH())
			continue;
		const std::vector<unsigned int> &start_slot_offsets = pair.second;
		ReservationTable *table = mac->getReservationManager()->getReservationTable(channel);
		// for each starting slot offset
		for (unsigned int start_slot_offset : start_slot_offsets) {
			// for each transmission burst
			for (unsigned int burst = 0; burst < timeout; burst++) {
				// for each of the link initiator's transmission slot
				for (unsigned int tx_slot = 0; tx_slot < burst_length_tx; tx_slot++) {
					unsigned int slot_offset = start_slot_offset + burst*burst_offset + tx_slot;
					try {
						table->lock_either_id(slot_offset, id_link_initiator, id_link_recipient);
						locked_resources_for_initiator.add_locked_resource(table, slot_offset);
					} catch (const std::exception &e) {
						std::stringstream ss;
						ss << *mac << "::" << *this << " couldn't lock link initiator's (id=" << id_link_initiator << ") TX slot at t=" << slot_offset << " on f=" << *channel << ": " << e.what();
						throw std::runtime_error(ss.str());
					}					
				}
				// for each of the link recipient's transmission slot
				for (unsigned int tx_slot = 0; tx_slot < burst_length_rx; tx_slot++) {
					unsigned int slot_offset = start_slot_offset + burst*burst_offset + burst_length_tx + tx_slot;
					try {
						table->lock_either_id(slot_offset, id_link_recipient, id_link_initiator);
						locked_resources_for_recipient.add_locked_resource(table, slot_offset);
					} catch (const std::exception &e) {
						std::stringstream ss;
						ss << *mac << "::" << *this << " couldn't lock link recipient's (id=" << id_link_initiator << ") TX slot at t=" << slot_offset << " on f=" << *channel << ": " << e.what();
						throw std::runtime_error(ss.str());
					}					
				}
			}
		}
	}
	coutd << "locked " << locked_resources_for_initiator.size() << " initiator resources and " << locked_resources_for_recipient.size() << " recipient resources -> ";
}

void ThirdPartyLink::processLinkReplyMessage(const L2HeaderLinkReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {	
	coutd << *this << " processing link reply -> ";			
	// reset	
	coutd << "attempting to unlock " << locked_resources_for_initiator.size() << " initator locks: unlocked " << locked_resources_for_initiator.unlock_either_id(id_link_initiator, id_link_recipient) << " -> ";	
	locked_resources_for_initiator.reset();		
	coutd << "attempting to unlock " << locked_resources_for_recipient.size() << " recipient locks: unlocked " << locked_resources_for_recipient.unlock_either_id(id_link_recipient, id_link_initiator) << " -> ";		
	locked_resources_for_recipient.reset();	
	// parse selected resource
	const std::map<const FrequencyChannel*, std::vector<unsigned int>>& selected_resource_map = payload->resources;
	if (selected_resource_map.size() != size_t(1))
		throw std::invalid_argument("PPLinkManager::processLinkReplyMessage got a reply that does not contain just one selected resource, but " + std::to_string(selected_resource_map.size()));
	const auto &selected_resource = *selected_resource_map.begin();
	const FrequencyChannel *selected_freq_channel = selected_resource.first;
	if (selected_resource.second.size() != size_t(1)) 
		throw std::invalid_argument("PPLinkManager::processLinkReplyMessage got a reply that does not contain just one time slot offset, but " + std::to_string(selected_resource.second.size()));
	const unsigned int selected_time_slot_offset = selected_resource.second.at(0);			
	const unsigned int &timeout = header->timeout, 
				&burst_length = header->burst_length,
				&burst_length_tx = header->burst_length_tx,
				burst_length_rx = header->burst_length - header->burst_length_tx,
				&burst_offset = header->burst_offset;
	unsigned int first_burst_in = selected_time_slot_offset - reply_offset; // normalize to current time slot
	const MacId initiator_id = header->getDestId(), &recipient_id = origin_id;
	// schedule the link's resources		
	try {
		this->scheduled_resources = mac->getReservationManager()->schedule_bursts(selected_freq_channel, timeout, first_burst_in, burst_length, burst_length_tx, burst_length_rx, burst_offset, initiator_id, recipient_id, false, true);		
	} catch (const std::exception &e) {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::processLinkReplyMessage couldn't schedule resources along this link: " << e.what();
		throw std::runtime_error(ss.str());
	}
	// reset counters
	num_slots_until_expected_link_reply = UNSET;
	reply_offset = UNSET;
	// set new counter
	link_expiry_offset = first_burst_in + (timeout-1)*burst_offset + burst_length - 1;	
}