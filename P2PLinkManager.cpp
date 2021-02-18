//
// Created by seba on 2/18/21.
//

#include <set>
#include "P2PLinkManager.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

P2PLinkManager::P2PLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int default_timeout, unsigned int burst_offset)
	: LinkManager(link_id, reservation_manager, mac), default_timeout(default_timeout), burst_offset(burst_offset), outgoing_traffic_estimate(burst_offset) {}

P2PLinkManager::~P2PLinkManager() {
	delete current_link_state;
	delete next_link_state;
}

std::map<const FrequencyChannel*, std::vector<unsigned int>> P2PLinkManager::p2pSlotSelection(unsigned int num_channels, unsigned int num_slots, unsigned int min_offset, unsigned int burst_length, unsigned int burst_length_tx, bool is_init) {
	auto proposal_map = std::map<const FrequencyChannel*, std::vector<unsigned int>>();
	// ... get the P2P reservation tables sorted by their numbers of idle slots ...
	auto table_priority_queue = reservation_manager->getSortedP2PReservationTables();
	// ... until we have considered the target number of channels ...
	coutd << "p2pSlotSelection to reserve " << burst_length << " slots -> ";
	for (size_t num_channels_considered = 0; num_channels_considered < num_channels; num_channels_considered++) {
		if (table_priority_queue.empty())
			break;
		// ... get the next reservation table ...
		ReservationTable* table = table_priority_queue.top();
		table_priority_queue.pop();
		// ... check if the channel is blocked ...
		if (table->getLinkedChannel()->isBlocked()) {
			num_channels_considered--;
			continue;
		}
		// ... and try to find candidate slots,
		std::vector<unsigned int> candidate_slots = table->findCandidates(num_slots, min_offset, burst_length, burst_length_tx, is_init);
		coutd << "found " << candidate_slots.size() << " slots on " << *table->getLinkedChannel() << ": ";
		for (int32_t slot : candidate_slots)
			coutd << slot << ":" << slot + burst_length - 1 << " ";
		coutd << " -> ";

		// ... if this is used for an initial link request, then we need to reserve a receiver at each start slot.
		if (is_init) {
			for (unsigned int offset : candidate_slots) {
				bool could_lock_receiver = false;
				for (auto* rx_table : rx_tables)
					if (rx_table->canLock(offset)) {
						rx_table->lock(offset);
						could_lock_receiver = true;
						break;
					}
				if (!could_lock_receiver)
					throw std::range_error("P2PLinkManager::p2pSlotSelection cannot reserve any receiver for first slot of burst.");
			}
		}
		// ... and lock them s.t. future proposals don't consider them.
		lock(candidate_slots, burst_length, burst_length_tx, table);
		coutd << "locked -> ";

		// Fill proposal.
		for (unsigned int slot : candidate_slots)
			proposal_map[table->getLinkedChannel()].push_back(slot);
	}
	return proposal_map;
}

void P2PLinkManager::onPacketReception(L2Packet*& packet) {

}

void P2PLinkManager::onReceptionBurstStart(unsigned int burst_length) {

}

void P2PLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {

}

L2Packet* P2PLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	return nullptr;
}

void P2PLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {

}

void P2PLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	// Update outgoing traffic estimate.
	outgoing_traffic_estimate.put(num_bits);

	if (link_status == link_not_established) {
		coutd << "link not established, triggering link establishment -> ";
		establishLink();
	} else
		coutd << "link status is '" << link_status << "'; nothing to do." << std::endl;
}

void P2PLinkManager::onSlotStart(uint64_t num_slots) {

}

void P2PLinkManager::onSlotEnd() {

}

void P2PLinkManager::establishLink() {
	auto *request = new L2Packet();
	// Add base header.
	request->addMessage(new L2HeaderBase(mac->getMacId(), 0, 0, 0), nullptr);
	// Add broadcast header.
	request->addMessage(new L2HeaderBroadcast(), nullptr);
	// Add request header.
	request->addMessage(new L2HeaderLinkRequest(link_id), nullptr);
	request->addCallback(this);
}

void P2PLinkManager::packetBeingSentCallback(TUHH_INTAIRNET_MCSOTDMA::L2Packet* packet) {

}
