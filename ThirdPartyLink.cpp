//
// Created by Sebastian Lindner on 01/12/22.
//

#include "ThirdPartyLink.hpp"
#include "SHLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ThirdPartyLink::ThirdPartyLink(const MacId& id_link_initiator, const MacId& id_link_recipient, MCSOTDMA_Mac *mac) 
	: id_link_initiator(id_link_initiator), id_link_recipient(id_link_recipient), reservation_map_for_link_initator(), reservation_map_for_link_recipient(), mac(mac) {}

void ThirdPartyLink::onSlotStart(size_t num_slots) {
	for (size_t t = 0; t < num_slots; t++) {
		reservation_map_for_link_initator.onSlotStart(); 
		reservation_map_for_link_recipient.onSlotStart();
	}
	if (this->num_slots_until_expected_link_reply != UNSET) {
		if (this->num_slots_until_expected_link_reply < num_slots)
			throw std::runtime_error("ThidPartyLink::onSlotStart attempted to decrement counter until expected link reply past zero.");
		this->num_slots_until_expected_link_reply -= num_slots;		
	}
}

void ThirdPartyLink::onSlotEnd() {
	// was a link reply expected this slot?
	if (num_slots_until_expected_link_reply == 0) {
		coutd << *mac << "::" << *this << " expected link reply hasn't arrived -> ";
		// unlock all locks
		reservation_map_for_link_initator.unlock_either_id(id_link_initiator, id_link_recipient);
		reservation_map_for_link_recipient.unlock_either_id(id_link_recipient, id_link_initiator);		
		// reset counter
		num_slots_until_expected_link_reply = UNSET;
	}
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
	if (reservation_map_for_link_initator.size() > 0) {
		reservation_map_for_link_initator.unlock(id_link_initiator);
		reservation_map_for_link_initator.unschedule();
		reservation_map_for_link_initator.clear();
	}
	if (reservation_map_for_link_recipient.size() > 0) {
		reservation_map_for_link_recipient.unlock(id_link_recipient);
		reservation_map_for_link_recipient.unschedule();
		reservation_map_for_link_recipient.clear();
	}
	// parse expected reply slot
	this->num_slots_until_expected_link_reply = (int) header->reply_offset;
	// check for a potential collision with our own broadcast
	auto *sh_manager = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
	if (sh_manager->isNextBroadcastScheduled()) 
		if (sh_manager->getNextBroadcastSlot() == num_slots_until_expected_link_reply) {
			coutd << "detected collision of advertised reply slot and our own broadcast -> ";
			sh_manager->reportCollisionWithScheduledBroadcast(id_link_initiator);
		}
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
					} catch (const std::exception &e) {
						std::stringstream ss;
						ss << *mac << "::" << *this << " couldn't lock link initiator's (id=" << id_link_initiator << ") TX slot at t=" << slot_offset << " on f=" << *channel << ": " << e.what();
						throw std::runtime_error(ss.str());
					}
					reservation_map_for_link_initator.add_locked_resource(table, slot_offset);
				}
				// for each of the link recipient's transmission slot
				for (unsigned int tx_slot = 0; tx_slot < burst_length_rx; tx_slot++) {
					unsigned int slot_offset = start_slot_offset + burst*burst_offset + burst_length_tx + tx_slot;
					try {
						table->lock_either_id(slot_offset, id_link_recipient, id_link_initiator);
					} catch (const std::exception &e) {
						std::stringstream ss;
						ss << *mac << "::" << *this << " couldn't lock link recipient's (id=" << id_link_initiator << ") TX slot at t=" << slot_offset << " on f=" << *channel << ": " << e.what();
						throw std::runtime_error(ss.str());
					}
					reservation_map_for_link_recipient.add_locked_resource(table, slot_offset);
				}
			}
		}
	}
	coutd << "locked " << reservation_map_for_link_initator.size() << " initiator resources and " << reservation_map_for_link_recipient.size() << " recipient resources -> ";
}

void ThirdPartyLink::processLinkReplyMessage(const L2HeaderLinkEstablishmentReply*& header, const LinkManager::LinkEstablishmentPayload*& payload) {

}