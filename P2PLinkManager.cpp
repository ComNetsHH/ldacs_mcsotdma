//
// Created by seba on 2/18/21.
//

#include <set>
#include <cassert>
#include <sstream>
#include "P2PLinkManager.hpp"
#include "coutdebug.hpp"
#include "BCLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

P2PLinkManager::P2PLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int default_timeout, unsigned int burst_offset)
	: LinkManager(link_id, reservation_manager, mac), default_timeout(default_timeout), burst_offset(burst_offset), outgoing_traffic_estimate(burst_offset),
	str_statistic_num_links_established("MCSOTDMA:statistic_num_links_established(" + std::to_string(link_id.getId()) + ")") {}

P2PLinkManager::~P2PLinkManager() {
	delete current_link_state;
}

std::pair<std::map<const FrequencyChannel*, std::vector<unsigned int>>, P2PLinkManager::LockMap> P2PLinkManager::p2pSlotSelection(unsigned int num_channels, unsigned int num_slots, unsigned int min_offset, unsigned int burst_length, unsigned int burst_length_tx) {
	auto proposal_map = std::map<const FrequencyChannel*, std::vector<unsigned int>>();
	auto locked_resources_map = LockMap();
	// ... get the P2P reservation tables sorted by their numbers of idle slots ...
	auto table_priority_queue = reservation_manager->getSortedP2PReservationTables();
	// ... until we have considered the target number of channels ...
	coutd << "p2pSlotSelection to reserve " << burst_length << " slots -> ";
	for (size_t num_channels_considered = 0; num_channels_considered < num_channels;) {
		if (table_priority_queue.empty())
			break;
		// ... get the next reservation table ...
		ReservationTable* table = table_priority_queue.top();
		table_priority_queue.pop();
		// ... check if the channel is blocked ...
		if (table->getLinkedChannel()->isBlocked()) 			
			continue;		
		// ... and try to find candidate slots,
		std::vector<unsigned int> candidate_slots = table->findCandidates(num_slots, min_offset, burst_offset, burst_length, burst_length_tx, default_timeout, true);

		coutd << "found " << candidate_slots.size() << " slots on " << *table->getLinkedChannel() << ": ";
		for (int32_t slot : candidate_slots)
			coutd << slot << ":" << slot + burst_length - 1 << " ";
		coutd << " -> ";

		// ... and lock them s.t. other proposals don't consider them.
		try {
			locked_resources_map.merge(lock_bursts(candidate_slots, burst_length, burst_length_tx, default_timeout, true, table));
		} catch (const std::exception& e) {
			throw std::runtime_error("Error during P2P slot selection: " + std::string(e.what()));
		}

		// Fill proposal.
		for (unsigned int slot : candidate_slots)
			proposal_map[table->getLinkedChannel()].push_back(slot);
		num_channels_considered++;
	}
	return {proposal_map, locked_resources_map};
}

P2PLinkManager::LockMap P2PLinkManager::lock_bursts(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, unsigned int timeout, bool is_link_initiator, ReservationTable* table) {
	coutd << "locking: ";
	// Bursts can be overlapping, so while we check that we *can* lock them, save the unique slots to save some processing steps.
	std::set<unsigned int> unique_offsets_tx, unique_offsets_rx, unique_offsets_local;

	// 1st: check that slots *can* be locked.
	for (auto burst_start_offset : start_slots) {
		// Go over all bursts of the entire link.
		for (unsigned int n_burst = 0; n_burst < timeout + 1; n_burst++) {
			// The first burst is where a link reply is expected...
			if (n_burst == 0) {
				// ... so lock the resource locally,
				if (!table->canLock(burst_start_offset)) {
					const Reservation &conflict_res = table->getReservation((int) burst_start_offset);
					std::stringstream ss;
					ss << *mac << "::" << *this << "::lock_bursts cannot lock local ReservationTable for first burst at t=" << burst_start_offset << ", conflict with " << conflict_res << ".";
					throw std::range_error(ss.str());
				}
				unique_offsets_local.emplace(burst_start_offset);
				// ... and at a receiver if the caller is the link initiator (who has to *receive* the link reply)
				if (is_link_initiator) {
					if (!std::any_of(rx_tables.begin(), rx_tables.end(), [burst_start_offset](ReservationTable* rx_table) { return rx_table->canLock(burst_start_offset); })) {
						Reservation conflict_res = Reservation();
						for (size_t i = 0; i < rx_tables.size() && conflict_res.isIdle(); i++)
							conflict_res = rx_tables.at(i)->getReservation((int) burst_start_offset);
						std::stringstream ss;
						ss << *mac << "::" << *this << "::lock_bursts cannot lock RX ReservationTable for first burst at t=" << burst_start_offset << ", conflict with " << conflict_res << ".";
						throw std::range_error(ss.str());
					}
					unique_offsets_rx.emplace(burst_start_offset);
				// ... or at a transmitter if the caller is the communication partner (who has to *send* the link reply)
				} else {
					if (!mac->isTransmitterIdle(burst_start_offset, 1)) {
						Reservation conflict_res = Reservation();
						for (size_t i = 0; i < tx_tables.size() && conflict_res.isIdle(); i++)
							conflict_res = tx_tables.at(i)->getReservation((int) burst_start_offset);
						std::stringstream ss;
						ss << *mac << "::" << *this << "::lock_bursts cannot lock TX ReservationTable for first burst at t=" << burst_start_offset << ", conflict with " << conflict_res << ".";
						throw std::range_error(ss.str());
					}
					unique_offsets_tx.emplace(burst_start_offset);
				}
			// Later ones are data transmissions...
			} else {
				// the first burst_length_tx slots...
				for (unsigned int t = 0; t < burst_length_tx; t++) {
					unsigned int offset = burst_start_offset + n_burst*burst_offset + t;
					// ... should be lockable locally
					if (!table->canLock(offset)) {
						const Reservation &conflict_res = table->getReservation((int) offset);
						std::stringstream ss;
						ss << *mac << "::" << *this << "::lock_bursts cannot lock local ReservationTable for later burst " << n_burst << "/" << timeout+1 << " at t=" << offset << ", conflict with " << conflict_res << ".";
						throw std::range_error(ss.str());
					}
					unique_offsets_local.emplace(offset);
					// Link initiators transmit during the first slots
					if (is_link_initiator) {
						// ... and at the transmitter
						if (!std::any_of(tx_tables.begin(), tx_tables.end(), [offset](ReservationTable* tx_table) { return tx_table->canLock(offset); })) {
							Reservation conflict_res = Reservation();
							for (auto it = tx_tables.begin(); it != tx_tables.end() && conflict_res.isIdle(); it++) {
								const auto tx_table = *it;
								if (!tx_table->getReservation(offset).isIdle()) {
									conflict_res = tx_table->getReservation(offset);
								}
							}
							std::stringstream ss;
							ss << *mac << "::" << *this << "::lock_bursts cannot lock TX ReservationTable for later burst " << n_burst << "/" << timeout+1 << " at t=" << offset << ", conflict with " << conflict_res << ".";
							throw std::range_error(ss.str());
						}
						unique_offsets_tx.emplace(offset);
					// Link recipients receive
					} else {
						if (!std::any_of(rx_tables.begin(), rx_tables.end(), [offset](ReservationTable* rx_table) { return rx_table->canLock(offset); })) {
							Reservation conflict_res = Reservation();
							for (auto it = rx_tables.begin(); it != rx_tables.end() && conflict_res.isIdle(); it++) {
								const auto rx_table = *it;
								if (!rx_table->getReservation(offset).isIdle()) {
									conflict_res = rx_table->getReservation(offset);
								}
							}
							std::stringstream ss;
							ss << *mac << "::" << *this << "::lock_bursts cannot lock RX ReservationTable for later burst " << n_burst << "/" << timeout+1 << " at t=" << offset << ", conflict with " << conflict_res << ".";
							throw std::range_error(ss.str());
						}
						unique_offsets_rx.emplace(offset);
					}
				}
				// Latter burst_length_rx slots...
				for (unsigned int t = burst_length_tx; t < burst_length; t++) {
					unsigned int offset = burst_start_offset + n_burst*burst_offset + t;
					// ... should be lockable locally
					if (!table->canLock(offset)) {
						const Reservation &conflict_res = table->getReservation((int) offset);
						std::stringstream ss;
						ss << *mac << "::" << *this << "::lock_bursts cannot lock local ReservationTable for later burst " << n_burst << "/" << timeout+1 << " at t=" << offset << ", conflict with " << conflict_res << ".";
						throw std::range_error(ss.str());
					}
					unique_offsets_local.emplace(offset);					
					// Link initiators receive during the later slots
					if (is_link_initiator) {
						if (!std::any_of(rx_tables.begin(), rx_tables.end(), [offset](ReservationTable* rx_table) { return rx_table->canLock(offset); })) {
							Reservation conflict_res = Reservation();
							for (auto it = rx_tables.begin(); it != rx_tables.end() && conflict_res.isIdle(); it++) {
								const auto rx_table = *it;
								if (!rx_table->getReservation(offset).isIdle()) {
									conflict_res = rx_table->getReservation(offset);
								}
							}
							std::stringstream ss;
							ss << *mac << "::" << *this << "::lock_bursts cannot lock RX ReservationTable for later burst " << n_burst << "/" << timeout+1 << "at t=" << offset << ", conflict with " << conflict_res << ".";
							throw std::range_error(ss.str());
						}
						unique_offsets_rx.emplace(offset);
					// Link recipients transmit
					} else {
						if (!std::any_of(tx_tables.begin(), tx_tables.end(), [offset](ReservationTable* tx_table) { return tx_table->canLock(offset); })) {
							Reservation conflict_res = Reservation();
							for (auto it = tx_tables.begin(); it != tx_tables.end() && conflict_res.isIdle(); it++) {
								const auto tx_table = *it;
								if (!tx_table->getReservation(offset).isIdle()) {
									conflict_res = tx_table->getReservation(offset);
								}
							}
							std::stringstream ss;
							ss << *mac << "::" << *this << "::lock_bursts cannot lock TX ReservationTable for later burst " << n_burst << "/" << timeout+1 << " at t=" << offset << ", conflict with " << conflict_res << ".";
							throw std::range_error(ss.str());
						}
						unique_offsets_tx.emplace(offset);
					}
				}
			}
		}
	}

	// 2nd: actually lock them.
	LockMap locked_resources_map = LockMap();
	// *All* slots should be locked in the local ReservationTable.
	for (unsigned int offset : unique_offsets_local) {		
		table->lock(offset);
		locked_resources_map.locks_local.push_back({table, offset});
	}
	// Then lock transmitter resources.
	for (unsigned int offset : unique_offsets_tx) {
		for (auto* tx_table : tx_tables)
			if (tx_table->canLock(offset)) {
				tx_table->lock(offset);
				locked_resources_map.locks_transmitter.push_back({tx_table, offset});
				break;
			}
	}
	// Then receiver resources.
	for (unsigned int offset : unique_offsets_rx) {
		for (auto* rx_table : rx_tables)
			if (rx_table->canLock(offset)) {
				rx_table->lock(offset);
				locked_resources_map.locks_receiver.push_back({rx_table, offset});
				break;
			}
	}
	coutd << unique_offsets_local.size() << " local + " << unique_offsets_rx.size() << " receiver + " << unique_offsets_tx.size() << " transmitter resources -> ";
	return locked_resources_map;
}

void P2PLinkManager::onReceptionBurstStart(unsigned int burst_length) {
	communication_during_this_slot = true;
	if (this->close_link_early_if_no_first_data_packet_comes_in && this->current_link_state != nullptr && link_status == Status::awaiting_data_tx) 
		this->current_link_state->num_failed_receptions_before_link_establishment++;
}

void P2PLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {
	communication_during_this_slot = true;
}

L2Packet* P2PLinkManager::onTransmissionBurstStart(unsigned int remaining_burst_length) {
	communication_during_this_slot = true;
	const unsigned int total_burst_length = remaining_burst_length + 1;

	coutd << *this << "::onTransmissionBurstStart(" << total_burst_length << " slots) -> ";
	if (link_status == link_not_established)
		throw std::runtime_error("P2PLinkManager::onTransmissionBurst for unestablished link.");

	auto *packet = new L2Packet();
	size_t capacity = mac->getCurrentDatarate() * total_burst_length;
	coutd << "filling packet with a capacity of " << capacity << " bits -> ";
	// Add base header.
	auto *base_header = new L2HeaderBase(mac->getMacId(), 0, 0, 0, 0);
	packet->addMessage(base_header, nullptr);
	coutd << "added " << base_header->getBits() << "-bit base header -> ";
	if (current_link_state != nullptr) {
		// Set base header fields.
		base_header->timeout = current_link_state->timeout;
		base_header->burst_length = current_link_state->burst_length;
		base_header->burst_length_tx = estimateCurrentNumSlots();
		base_header->burst_offset = burst_offset;

		// Put a priority on control messages:
		// 1) link replies
		if (!current_link_state->scheduled_link_replies.empty()) {
			for (auto it = current_link_state->scheduled_link_replies.begin(); it != current_link_state->scheduled_link_replies.end(); it++) {
				auto &reply_reservation = *it;
				// ... if due now, ...
				if (reply_reservation.getRemainingOffset() == 0) {
					size_t num_bits = reply_reservation.getHeader()->getBits() + reply_reservation.getPayload()->getBits();
					if (packet->getBits() + num_bits <= capacity) {
						// put it into the packet,
						packet->addMessage(reply_reservation.getHeader(), reply_reservation.getPayload());
						// and remove from scheduled replies.
						current_link_state->scheduled_link_replies.erase(it);
						it--;
						coutd << "added " << reply_reservation.getHeader()->getBits() + reply_reservation.getPayload()->getBits() << "-bit scheduled link reply to init link on " << *reply_reservation.getPayload()->proposed_resources.begin()->first << "@" << reply_reservation.getPayload()->proposed_resources.begin()->second.at(0) << " -> ";
						mac->statisticReportLinkReplySent();
						// schedule all link resources
						// Clear locked resources.
						if (lock_map.anyLocks()) {
							clearLockedResources(lock_map);
							lock_map = LockMap();
						}
						// Mark reservations.
						coutd << "reserving bursts: ";
						assert(current_link_state != nullptr);
						for (unsigned int burst = 1; burst < current_link_state->timeout + 1; burst++)
							try {
								scheduleBurst(burst * burst_offset, current_link_state->burst_length, current_link_state->burst_length_tx, link_id, current_reservation_table, current_link_state->is_link_initiator);
							} catch (const std::invalid_argument& e) {
								std::stringstream ss;
								ss << *mac << "::" << *this << "::processUnicastMessage conflict at t=" << burst*burst_offset << ": " << e.what() << "!";
								throw std::runtime_error(ss.str());
							}
					} else // Link replies must fit into single slots & have highest priority, so they should always fit. Throw an error if the unexpected happens.
						throw std::runtime_error("P2PLinkManager::onTransmissionBurstStart can't put link reply into packet because it wouldn't fit. This should never happen?!");
				}
			}
		}
	}
	// Fill whatever capacity remains with upper-layer data.
	unsigned int remaining_bits = capacity - packet->getBits() + base_header->getBits(); // The requested packet will have a base header, which we'll drop, so add it to the requested number of bits.
	coutd << "requesting " << remaining_bits << " bits from upper sublayer -> ";
	L2Packet *upper_layer_data = mac->requestSegment(remaining_bits, link_id);	
	mac->statisticReportUnicastSent();
	for (size_t i = 0; i < upper_layer_data->getPayloads().size(); i++)
		if (upper_layer_data->getHeaders().at(i)->frame_type != L2Header::base)
			packet->addMessage(upper_layer_data->getHeaders().at(i), upper_layer_data->getPayloads().at(i));
	return packet;
}

void P2PLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {
	communication_during_this_slot = true;
}

void P2PLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *mac << "::" << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	// Update outgoing traffic estimate.
	outgoing_traffic_estimate.put(num_bits);

	if (link_status == link_not_established) {
		link_status = awaiting_reply;
		coutd << "link not established, changing status to '" << link_status << "', triggering link establishment -> ";
		auto link_request_msg = prepareRequestMessage();
		((BCLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->sendLinkRequest(link_request_msg.first, link_request_msg.second);		
	} else
		coutd << "link status is '" << link_status << "'; nothing to do." << std::endl;
}

void P2PLinkManager::onSlotStart(uint64_t num_slots) {
	coutd << *mac << "::" << *this << "::onSlotStart(" << num_slots << ") -> ";
	communication_during_this_slot = false;
	updated_timeout_this_slot = false;
	established_initial_link_this_slot = false;
	established_link_this_slot = false;

	lock_map.num_slots_since_creation += num_slots;

	// TODO properly test this (not sure if incrementing time by this many slots works as intended right now)
	if (num_slots > burst_offset) {
		std::cerr << "incrementing time by this many slots is untested; I'm not stopping, just warning." << std::endl;
		int num_passed_bursts = num_slots / burst_offset;
		for (int i = 0; i < num_passed_bursts; i++) {
			if (decrementTimeout())
				onTimeoutExpiry();
			if (i < num_passed_bursts - 1)
				updated_timeout_this_slot = false;
		}
	}

	if (current_link_state != nullptr) {
		for (auto &reservation : current_link_state->scheduled_link_replies)
			reservation.update(num_slots);
		// Update RX reservations to listen for replies.
		for (auto it = current_link_state->scheduled_rx_slots.begin(); it != current_link_state->scheduled_rx_slots.end(); it++) {
			// If it has passed, remove it from the remembered ones.
			if (it->second < num_slots) {
				current_link_state->scheduled_rx_slots.erase(it);
				it--;
			} else {
				it->second -= num_slots;
			}
		}
		if (current_link_state->next_burst_start > 0)
			current_link_state->next_burst_start -= num_slots % burst_offset;

	}
}

void P2PLinkManager::onSlotEnd() {
	if (current_reservation_table != nullptr && communication_during_this_slot && current_reservation_table->isBurstEnd(0, link_id)) {
		coutd << *mac << "::" << *this << "::onSlotEnd -> ";
		if (decrementTimeout())
			onTimeoutExpiry();
		coutd << std::endl;
	}
	if (current_link_state != nullptr) {
		if (current_link_state->next_burst_start == 0)
			current_link_state->next_burst_start = burst_offset;
		// If we're awaiting a reply, make sure that we've not missed the latest reception opportunity.
		if (link_status == awaiting_reply && current_link_state->waiting_for_agreement) {
			if (current_link_state->latest_agreement_opportunity == 0) {
				coutd << *mac << "::" << *this << " missed last link establishment opportunity, resetting link -> ";
				// We've missed the latest opportunity. Reset the link status.
				terminateLink();
				// Increment statistic.
				mac->statistcReportPPLinkMissedLastReplyOpportunity();
				// Check if there's more data,
				if (mac->isThereMoreData(link_id)) // and re-establish the link if there is.
					notifyOutgoing((unsigned long) outgoing_traffic_estimate.get());
			} else
				current_link_state->latest_agreement_opportunity -= 1;
		}
		// If we're awaiting the first data transmission for too many slots, then terminate the link.
		if (close_link_early_if_no_first_data_packet_comes_in && link_status == Status::awaiting_data_tx && current_link_state->num_failed_receptions_before_link_establishment > mac->getUpperLayer()->getMaxNumRtxAttempts()) {
			coutd << *mac << "::" << *this << " has not received the first data transmission within too many slots, resetting link -> ";
			mac->statisticReportLinkClosedEarly();
			terminateLink();
			// Check if there's more data,
			if (mac->isThereMoreData(link_id)) // and re-establish the link if there is.
				notifyOutgoing((unsigned long) outgoing_traffic_estimate.get());
		}
	}
	// if the link has been established in this slot
	if (established_link_this_slot) {
		coutd << *mac << "::" << *this << "::onSlotEnd -> passing link info broadcast into broadcast queue -> ";
		// inject a LinkInfo into the upper layer
		auto *packet = new L2Packet();
		packet->addMessage(new L2HeaderBase(mac->getMacId(), 0, 1, 1, 0), nullptr);
		packet->addMessage(new L2HeaderLinkInfo(), new LinkInfoPayload(this));
		mac->injectIntoUpper(packet);
	}	
	LinkManager::onSlotEnd();
}

std::pair<L2HeaderLinkRequest*, LinkManager::LinkRequestPayload*> P2PLinkManager::prepareRequestMessage() {
	auto *header = new L2HeaderLinkRequest(link_id);
	auto *payload = new LinkRequestPayload();
	// Set this as the callback s.t. the payload can be populated just-in-time.
	payload->callback = this;
	return {header, payload};
}

std::pair<unsigned int, unsigned int> P2PLinkManager::getTxRxDistribution(unsigned int tx_slots_me, unsigned int tx_slots_you) {	
	unsigned int burst_length_tx = std::max(uint32_t(1), tx_slots_me); // in slots.
	unsigned int burst_length = burst_length_tx + tx_slots_you; // own transmission slots + those the communication partner desires
	// for very large amounts of data, this might exceed the frame length
	if (burst_length > burst_offset) {
		// what's the fraction of slots that I should reserve as TX?
		double my_tx_fraction = ((double) burst_length_tx) / ((double) burst_length);				
		burst_length_tx = (int) (my_tx_fraction * burst_offset);				
		burst_length = burst_offset;		
	}

	return {burst_length_tx, burst_length};
}

void P2PLinkManager::populateLinkRequest(L2HeaderLinkRequest*& header, LinkManager::LinkRequestPayload*& payload) {
	coutd << "populating link request -> ";
	unsigned int min_offset = 2;

	auto tx_rx_distribution = this->getTxRxDistribution(estimateCurrentNumSlots(), reported_desired_tx_slots);	
	unsigned int burst_length_tx = tx_rx_distribution.first;
	unsigned int burst_length = tx_rx_distribution.second;	

	coutd << "min_offset=" << min_offset << ", burst_length=" << burst_length << ", burst_length_tx=" << burst_length_tx << " -> ";
	// We want to populate the payload.
	// First, clear previously-made locks.
	if (lock_map.anyLocks())
		clearLockedResources(this->lock_map);
	const auto &proposed_locked_pair = p2pSlotSelection(num_p2p_channels_to_propose, num_slots_per_p2p_channel_to_propose, min_offset, burst_length, burst_length_tx);
	payload->proposed_resources = proposed_locked_pair.first;
	this->lock_map = proposed_locked_pair.second;
	// Populate header.
	header->timeout = default_timeout;
	header->burst_length = burst_length;
	header->burst_length_tx = burst_length_tx;
	header->burst_offset = burst_offset;
	// Save state.
	delete current_link_state;
	current_link_state = new LinkState(default_timeout, burst_length, burst_length_tx);
	current_link_state->is_link_initiator = true;
	current_link_state->initial_setup = true;
	current_link_state->time_when_request_was_sent = mac->getCurrentSlot();
	// We need to schedule RX slots at each candidate to be able to receive a reply there.
	for (const auto &pair : payload->proposed_resources) {
		const FrequencyChannel *channel = pair.first;
		const std::vector<unsigned int> &burst_start_offsets = pair.second;
		ReservationTable *table = reservation_manager->getReservationTable(channel);
		for (unsigned int offset : burst_start_offsets) {
			table->mark(offset, Reservation(link_id, Reservation::RX));
			// Remember them.
			current_link_state->scheduled_rx_slots.emplace_back(channel, offset);
		}
	}
	// Remember the latest slot where a reply could be received.
	current_link_state->latest_agreement_opportunity = payload->getLatestProposedSlot();
	current_link_state->waiting_for_agreement = true;

	coutd << "request populated -> ";
}

bool P2PLinkManager::isProposalViable(const ReservationTable* table, unsigned int burst_start, unsigned int burst_length, unsigned int burst_length_tx, unsigned int burst_offset, unsigned int timeout) const {
	// Should be able to transmit reply at initial slot.
	bool viable = table->isIdle((int) burst_start, 1) && mac->isTransmitterIdle(burst_start, 1);
	// Then for each communication burst...
	if (viable)
		for (unsigned int burst = 1; viable && burst < timeout + 1; burst++) {
			int slot = (int) (burst_start + burst*burst_offset);
			unsigned int burst_length_rx = burst_length - burst_length_tx;
			// Entire slot range must be idle && receiver during first slots && transmitter during later ones.
			viable = viable && table->isIdle(slot, burst_length)
						&& mac->isAnyReceiverIdle(slot, burst_length_tx)
						&& mac->isTransmitterIdle(slot + burst_length_tx, burst_length_rx);
		}
	return viable;
}

void P2PLinkManager::processLinkRequestMessage(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) {
	coutd << *mac << "::" << *this << "::processLinkRequestMessage -> ";
	mac->statisticReportLinkRequestReceived();
	// If currently the link is unestablished, then this request must be an initial request.
	if (link_status == link_not_established) {
		processIncomingLinkRequest_Initial(header, payload, origin);
	// If a link request had been prepared by this node and a link request arrives here
	// then reset the link, stop trying to send the local request, and process the remote request.
	} else if (link_status == awaiting_reply) {
		// Cancel buffered and unsent local link requests.
		size_t num_cancelled_requests = ((BCLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->cancelLinkRequest(link_id);
		coutd << "cancelled " << num_cancelled_requests << " link requests from local buffer -> ";
		mac->statisticReportCancelledLinkRequest(num_cancelled_requests);
		// Reset link.
		terminateLink();
		// Process request.
		processIncomingLinkRequest_Initial(header, payload, origin);
	// If the link is of any other status, there is nothing to do.
	} else {
		coutd << "link is not unestablished; ignoring -> ";
	}
}

void P2PLinkManager::processIncomingLinkRequest_Initial(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) {
	// Unlock any previously-locked resources, as a new link request re-starts this procedure.
	if (lock_map.anyLocks()) {
		clearLockedResources(lock_map);
		lock_map = LockMap();
	}
	try {
		// Pick a random resource from those proposed.
		LinkState* state = selectResourceFromRequest((const L2HeaderLinkRequest*&) header, (const P2PLinkManager::LinkRequestPayload*&) payload);
		state->initial_setup = true;
		// remember the choice,
		delete current_link_state;
		current_link_state = state;
		current_channel = current_link_state->channel;
		current_reservation_table = reservation_manager->getReservationTable(current_channel);
		coutd << "randomly chose " << current_link_state->next_burst_start << "@" << *current_channel << " -> ";
		// lock all resources of the link
		coutd << "locking resources on entire link: ";
		try {
			this->lock_map = lock_bursts({current_link_state->next_burst_start}, current_link_state->burst_length, current_link_state->burst_length_tx, current_link_state->timeout, false, current_reservation_table);
			coutd << lock_map.size_local() << " local, " << lock_map.size_receiver() << " receiver and " << lock_map.size_transmitter() << " transmitter resources were locked -> ";
		} catch (const std::exception& e) {
			throw std::runtime_error("Error during link request processing: " + std::string(e.what()));
		}
		// schedule a link reply,
		auto link_reply_message = prepareReply(origin, current_link_state->channel, current_link_state->next_burst_start, current_link_state->burst_length, current_link_state->burst_length_tx);
		current_link_state->scheduled_link_replies.emplace_back(state->next_burst_start, (L2Header*&) link_reply_message.first, (LinkRequestPayload*&) link_reply_message.second);
		// mark the slot as TX,
		current_reservation_table->mark(state->next_burst_start, Reservation(origin, Reservation::TX));
		coutd << "scheduled link reply at offset " << state->next_burst_start << " -> ";
		// and anticipate first data exchange one burst later,
		coutd << "scheduling slots for first transmission burst: ";
		try {
			scheduleBurst(burst_offset + current_link_state->next_burst_start, current_link_state->burst_length, current_link_state->burst_length_tx, origin, current_reservation_table, current_link_state->is_link_initiator);
		} catch (const std::invalid_argument& e) {
			std::stringstream ss;
			ss << *mac << "::" << *this << "::processLinkRequestMessage conflict at t=" << burst_offset + current_link_state->next_burst_start << ": " << e.what() << "!";
			throw std::runtime_error(ss.str());
		}
		// and update status.
		coutd << "changing status " << link_status << "->" << awaiting_data_tx << " -> ";
		link_status = awaiting_data_tx;
	} catch (const std::invalid_argument& e) {
		coutd << "error during link request processing: " << e.what() << " -> assuming no viable resources; aborting -> ";
	}
}

std::pair<const FrequencyChannel*, unsigned int> P2PLinkManager::chooseRandomResource(const std::map<const FrequencyChannel*, std::vector<unsigned int>>& resources, unsigned int burst_length, unsigned int burst_length_tx) {
	std::vector<const FrequencyChannel*> viable_resource_channel;
	std::vector<unsigned int> viable_resource_slot;
	// For each resource...
	for (const auto &resource : resources) {
		const FrequencyChannel *channel = resource.first;
		const std::vector<unsigned int> &slots = resource.second;
		// ... get the channel's ReservationTable
		const ReservationTable *table = reservation_manager->getReservationTable(channel);
		// ... and check all proposed slot ranges, saving viable ones.
		coutd << "checking ";
		for (unsigned int slot : slots) {
			coutd << slot << "@" << *channel << " ";
			// ... isViableProposal checks that the range is idle and hardware available
			if (isProposalViable(table, slot, burst_length, burst_length_tx, burst_offset, default_timeout)) {
				viable_resource_channel.push_back(channel);
				viable_resource_slot.push_back(slot);
				coutd << "(viable) ";
			} else
				coutd << "(busy) ";
		}
	}
	if (viable_resource_channel.empty())
		throw std::invalid_argument("No viable resources were provided.");
	else {
		auto random_index = getRandomInt(0, viable_resource_channel.size());
		return {viable_resource_channel.at(random_index), viable_resource_slot.at(random_index)};
	}
}

P2PLinkManager::LinkState* P2PLinkManager::selectResourceFromRequest(const L2HeaderLinkRequest*& header, const LinkManager::LinkRequestPayload*& payload) {
	// Parse header fields.
	auto *state = new LinkState(header->timeout, header->burst_length, header->burst_length_tx);
	// Since this user is processing the request, they have not initiated the link.
	state->is_link_initiator = false;

	// Parse proposed resources.
	try {
		auto chosen_resource = chooseRandomResource(payload->proposed_resources, header->burst_length, header->burst_length_tx);
		// ... and save it.
		state->channel = chosen_resource.first;
		state->next_burst_start = chosen_resource.second;
		return state;
	} catch (const std::invalid_argument& e) {
		// If no resources were viable, forward this error.
		throw e;
	}
}

void P2PLinkManager::processLinkReplyMessage(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& message_payload) {
	coutd << *this << "::processLinkReplyMessage -> ";
	mac->statisticReportLinkReplyReceived();
	if (link_status != awaiting_reply) {
		coutd << "not awaiting reply; discarding -> ";
		return;
	}
	assert(current_link_state != nullptr && "P2PLinkManager::processLinkReplyMessage for unset current state.");

	const auto*& payload = (const LinkManager::LinkRequestPayload*&) message_payload;

	current_link_state->is_link_initiator = true;
	// Reset timeout.
	current_link_state->timeout = default_timeout;
	// Parse resource.
	if (payload->proposed_resources.size() != 1)
		throw std::invalid_argument("P2PLinkManager::processInitialReply for payload with " + std::to_string(payload->proposed_resources.size()) + " resources.");
	const auto &resource = *payload->proposed_resources.begin();
	const auto *channel = resource.first;
	const auto &slots = resource.second;
	if (slots.size() != 1)
		throw std::invalid_argument("P2PLinkManager::processInitialReply for " + std::to_string(slots.size()) + " slots.");
	unsigned int slot_offset = slots.at(0);
	coutd << "received on " << *channel << "@" << slot_offset << " -> ";
	// Assign channel.
	assign(channel);
	// Clear locked resources.
	if (lock_map.anyLocks()) {
		clearLockedResources(lock_map);
		lock_map = LockMap();
	}
	// Make reservations.
	coutd << "scheduling transmission bursts: ";
	for (unsigned int burst = 1; burst < default_timeout + 1; burst++)  // Start with next P2P frame
		try {
			scheduleBurst(burst * burst_offset + slot_offset, current_link_state->burst_length, current_link_state->burst_length_tx, link_id, current_reservation_table, current_link_state->is_link_initiator);
		} catch (const std::invalid_argument& e) {
			std::stringstream ss;
			ss << *mac << "::" << *this << "::processLinkReplyMessage conflict at t=" << burst*burst_offset + slot_offset << ": " << e.what() << "!";
			throw std::runtime_error(ss.str());
		}
	// Clear RX reservations made to receive this reply.
	for (auto &pair : current_link_state->scheduled_rx_slots) {
		ReservationTable *table = reservation_manager->getReservationTable(pair.first);
		table->mark(pair.second, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
	}
	current_link_state->scheduled_rx_slots.clear();
	// Link is now established.
	coutd << "setting link status to '";
	link_status = link_established;
	int link_establishment_time = mac->getCurrentSlot() - current_link_state->time_when_request_was_sent;
	mac->statisticReportPPLinkEstablishmentTime(link_establishment_time);
	statistic_num_links_established++;
	mac->statisticReportPPLinkEstablished();
	established_initial_link_this_slot = true;
	established_link_this_slot = true;
	coutd << link_status << "' -> ";
	current_link_state->waiting_for_agreement = false;
}

std::pair<L2HeaderLinkReply*, LinkManager::LinkRequestPayload*> P2PLinkManager::prepareReply(const MacId& dest_id, const FrequencyChannel *channel, unsigned int slot_offset, unsigned int burst_length, unsigned int burst_length_tx) const {
	// The reply header.
	auto *header = new L2HeaderLinkReply(dest_id);
	header->timeout = default_timeout;
	header->burst_offset = burst_offset;
	header->burst_length = burst_length;
	header->burst_length_tx = burst_length_tx;
	// The reply payload encodes the single, chosen resource.
	auto *payload = new LinkRequestPayload();
	payload->proposed_resources[channel].push_back(slot_offset);
	return {header, payload};
}

void P2PLinkManager::scheduleBurst(unsigned int burst_start_offset, unsigned int burst_length, unsigned int burst_length_tx, const MacId &dest_id, ReservationTable* table, bool link_initiator) {
	assert(table != nullptr);
	for (unsigned int t = 0; t < burst_length_tx; t++) {
		Reservation::Action action = t==0 ? (link_initiator ? Reservation::Action::TX : Reservation::Action::RX) : (link_initiator ? Reservation::Action::TX_CONT : Reservation::Action::RX_CONT);
		Reservation res = Reservation(dest_id, action, burst_length_tx > 0 ? burst_length_tx - 1 : 0);
		try {
			table->mark(burst_start_offset + t, res);
			coutd << "t=" << burst_start_offset + t << ":" << res << " ";
		} catch (const no_tx_available_error& e) {
			Reservation res_tx;
			for (const auto pair : mac->getReservations(burst_start_offset + t)) {
				if (!pair.first.isIdle())
					res_tx = pair.first;
			}
			if (!res_tx.isBeaconTx()) {
				std::stringstream ss;
				ss << *mac << "::" << *this << "::scheduleBursts couldn't schedule " << res << " at t=" << burst_start_offset + t << " because there's a conflict with " << res_tx << "!";
				throw std::invalid_argument(ss.str());
			}
		}

	}
	unsigned int burst_length_rx = burst_length - burst_length_tx;
	for (unsigned int t = 0; t < burst_length_rx; t++) {
		Reservation::Action action = t==0 ? (link_initiator ? Reservation::Action::RX : Reservation::Action::TX) : (link_initiator ? Reservation::Action::RX_CONT : Reservation::Action::TX_CONT);
		Reservation res = Reservation(dest_id, action, burst_length_rx > 0 ? burst_length_rx - 1 : 0);
		try {
			table->mark(burst_start_offset + burst_length_tx + t, res);
			coutd << "t=" << burst_start_offset + burst_length_tx + t << ":" << res << " ";
		} catch (const no_tx_available_error& e) {
			Reservation res_tx;
			for (const auto pair : mac->getReservations(burst_start_offset + t)) {
				if (!pair.first.isIdle())
					res_tx = pair.first;
			}
			if (!res_tx.isBeaconTx()) {
				std::stringstream ss;
				ss << *mac << "::" << *this << "::scheduleBursts couldn't schedule " << res << " at t=" << burst_start_offset + t << " because there's a conflict with " << res_tx << "!";
				throw std::invalid_argument(ss.str());
			}
		}
	}
	coutd << "-> ";
}

void P2PLinkManager::processBeaconMessage(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	throw std::invalid_argument("P2PLinkManager::processBeaconMessage called but beacons should not be received on P2P channels.");
}

void P2PLinkManager::processBroadcastMessage(const MacId& origin, L2HeaderBroadcast*& header) {
	throw std::invalid_argument("P2PLinkManager::processBroadcastMessage called but broadcasts should not be received on P2P channels.");
}

void P2PLinkManager::processUnicastMessage(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	MacId dest_id = header->dest_id;
	if (dest_id != mac->getMacId()) {
		coutd << "discarding unicast message not intended for us -> ";
		return;
	} else {
		mac->statisticReportUnicastMessageDecoded();
		
		if (link_status == awaiting_data_tx) {
			// Link is now established.
			link_status = link_established;
			statistic_num_links_established++;
			mac->statisticReportPPLinkEstablished();
			established_link_this_slot = true;
			coutd << "this transmission establishes the link, setting status to '" << link_status << "' -> informing upper layers -> ";
			// Inform upper sublayers.
			mac->notifyAboutNewLink(link_id);			
		}
	}
}

void P2PLinkManager::processBaseMessage(L2HeaderBase*& header) {
	// The communication partner informs about its *current wish* for their own burst length.
	this->setReportedDesiredTxSlots(header->burst_length_tx);
	mac->reportNeighborActivity(header->src_id);
}

bool P2PLinkManager::decrementTimeout() {
	// Don't decrement timeout if,
	// (1) the link is not established right now
	if (link_status == LinkManager::link_not_established || current_link_state == nullptr) {
		coutd << "link not established; not decrementing timeout -> ";
		return false;
	}
	// (2) we are in the process of initial establishment.
	if (link_status == LinkManager::awaiting_reply || link_status == LinkManager::awaiting_data_tx) {
		coutd << "link being established; not decrementing timeout -> ";
		return false;
	}
	// (3) it has already been updated this slot.
	if (updated_timeout_this_slot) {
		coutd << "already decremented timeout this slot; not decrementing timeout -> ";
		return current_link_state->timeout == 0;
	}
	// (4) the link was just now established.
	if (established_initial_link_this_slot) {
		coutd << "link was established in this slot; not decrementing timeout -> ";
		return current_link_state->timeout == 0;
	}

	updated_timeout_this_slot = true;

	if (current_link_state->timeout == 0)
		throw std::runtime_error("P2PLinkManager::decrementTimeout attempted to decrement timeout past zero.");
	coutd << "timeout " << current_link_state->timeout << "->";
	current_link_state->timeout--;
	coutd << current_link_state->timeout << " -> ";
	return current_link_state->timeout == 0;
}

void P2PLinkManager::onTimeoutExpiry() {
	coutd << "timeout reached -> ";
	coutd << "updating status: " << link_status << "->" << LinkManager::link_not_established << " -> cleared associated channel at " << *current_channel << " -> ";
	terminateLink();
	// Check if there's more data,
	if (mac->isThereMoreData(link_id)) // and re-establish the link if there is.
		notifyOutgoing((unsigned long) outgoing_traffic_estimate.get());
}

void P2PLinkManager::clearLocks(const std::vector<std::pair<ReservationTable*, unsigned int>>& locked_resources, unsigned int normalization_offset) {
	for (const auto& pair : locked_resources) {
		ReservationTable *table = pair.first;
		unsigned int slot = pair.second;		
		if (slot < normalization_offset) 			
			continue; // Skip those that have already passed.		
		unsigned int normalized_offset = slot - normalization_offset;		
		if (table->getLinkedChannel() != nullptr)
			coutd << "(t=" << normalized_offset << " f=" << table->getLinkedChannel()->getCenterFrequency() << "), ";
		if (table->getReservation((int) normalized_offset).isLocked())			
			table->mark((int) normalized_offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));		
	}
}

void P2PLinkManager::clearLockedResources(const LockMap& locked_resources) {
	coutd << "freeing " << locked_resources.size_local() << " local + " << locked_resources.size_receiver() << " receiver + " << locked_resources.size_transmitter() << " transmitter locks on resources ";	
	clearLocks(locked_resources.locks_local, locked_resources.num_slots_since_creation);
	clearLocks(locked_resources.locks_receiver, locked_resources.num_slots_since_creation);
	clearLocks(locked_resources.locks_transmitter, locked_resources.num_slots_since_creation);
	coutd << "-> ";
}

void P2PLinkManager::assign(const FrequencyChannel* channel) {
	// Base class call might set current_channel, but only if it's nullptr, so we do the same.
	if (current_channel == nullptr && current_link_state != nullptr)
		current_link_state->channel = channel;
	LinkManager::assign(channel);
}

unsigned int P2PLinkManager::estimateCurrentNumSlots() const {
	if (!mac->isThereMoreData(link_id))
		return 0;
	unsigned int traffic_estimate = (unsigned int) outgoing_traffic_estimate.get(); // in bits.
	unsigned int datarate = mac->getCurrentDatarate(); // in bits/slot.
	return std::max(uint32_t(0), traffic_estimate / datarate); // in slots.
}

unsigned int P2PLinkManager::getExpiryOffset() const {
	if (current_link_state == nullptr)
		return 0;
	else
		return (current_link_state->timeout - 1)*burst_offset + current_link_state->burst_length;
}

LinkInfo P2PLinkManager::getLinkInfo() {
	if (current_link_state == nullptr)
		throw std::runtime_error("P2PLinkManager::getLinkInfo for current_link_state == nullptr");
	MacId tx_id = current_link_state->is_link_initiator ? mac->getMacId() : link_id;
	MacId rx_id = current_link_state->is_link_initiator ? link_id : mac->getMacId();
	int offset;
	try {
		offset = getNumSlotsUntilNextBurst();
	} catch (const std::exception& e) {
		throw std::runtime_error("P2PLinkManager::getLinkInfo error: " + std::string(e.what()));
	}
	unsigned int timeout = current_link_state->timeout;
	if (isSlotPartOfBurst(0))
		timeout = timeout > 0 ? timeout-1 : timeout;
	LinkInfo info = LinkInfo(tx_id, rx_id, current_channel->getCenterFrequency(), offset, timeout, current_link_state->burst_length, current_link_state->burst_length_tx);
	coutd << info;
	return info;
}

void P2PLinkManager::processLinkInfoMessage(const L2HeaderLinkInfo*& header, const LinkInfoPayload*& payload) {
	const LinkInfo &info = payload->getLinkInfo();
	coutd << info << " -> ";
	const FrequencyChannel *channel = reservation_manager->getFreqChannelByCenterFreq(info.getP2PChannelCenterFreq());
	ReservationTable *table = reservation_manager->getReservationTable(channel);
	coutd << "f=" << *channel << ": ";
	for (int burst = 0; burst < info.getTimeout(); burst++) {
		for (int t = burst*((int) burst_offset) + info.getOffset(); t < burst*((int) burst_offset) + info.getOffset() + info.getBurstLength(); t++) {
			const Reservation& res = table->getReservation(t);
			coutd << "t=" << t << ":" << res << "->";
			if (res.isIdle()) {
				bool initiator_tx_range = t < burst*((int) burst_offset) + info.getOffset() + info.getBurstLengthTx();
				MacId id = initiator_tx_range ? info.getTxId() : info.getRxId();
				coutd << *table->mark(t, Reservation(id, Reservation::BUSY)) << " -> ";
			} else
				coutd << "skip -> ";
		}
	}
}

bool P2PLinkManager::isSlotPartOfBurst(int t) const {
	if (current_reservation_table == nullptr)
		throw std::runtime_error("P2PLinkManager::isSlotPartOfBurst for nullptr ReservationTable");
	const Reservation &res = current_reservation_table->getReservation(t);
	return res.getTarget() == link_id && (current_link_state->is_link_initiator ? (res.isTx() || res.isTxCont()) : (res.isRx() || res.isRxCont()));
}

int P2PLinkManager::getNumSlotsUntilNextBurst() const {
	if (current_reservation_table == nullptr || current_link_state == nullptr)
		throw std::runtime_error("P2PLinkManager::getNumSlotsUntilNextBurst for nullptr ReservationTable or LinkState.");
	else {
		// Proceed to after a potential current burst.
		int t = 1;
		while (isSlotPartOfBurst(t))
			t++;
		// Now look for the next burst start.
		for (;t < current_reservation_table->getPlanningHorizon(); t++) {
			const Reservation &res = current_reservation_table->getReservation(t);
			if (res.getTarget() == link_id && (current_link_state->is_link_initiator ? res.isTx() : res.isRx()))
				return t;
		}
	}
	throw std::range_error("P2PLinkManager::getNumSlotsUntilNextBurst can't find next burst.");
}

void P2PLinkManager::terminateLink() {
	// Clear any locks that may have been put.
	if (lock_map.anyLocks()) {
		clearLockedResources(this->lock_map);
		this->lock_map = LockMap();
	}
	// Reset states.
	current_channel = nullptr;
	current_reservation_table = nullptr;
	link_status = LinkManager::link_not_established;
	if (current_link_state != nullptr) {
		coutd << "clearing pending RX reservations: ";
		for (auto& pair : current_link_state->scheduled_rx_slots) {
			ReservationTable* table = reservation_manager->getReservationTable(pair.first);
			table->mark(pair.second, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
			coutd << pair.second << "@" << *pair.first << " ";
		}
	}
	delete current_link_state;
	current_link_state = nullptr;
	coutd << "link reset, status is " << link_status << " -> ";
}

void P2PLinkManager::setShouldTerminateLinksEarly(bool flag) {
	this->close_link_early_if_no_first_data_packet_comes_in = flag;
}

void P2PLinkManager::setReportedDesiredTxSlots(unsigned int value) {
	if (this->force_bidirectional_links)
		this->reported_desired_tx_slots = std::max(uint(1), value);
	else
		this->reported_desired_tx_slots = value;
}

void P2PLinkManager::setForceBidirectionalLinks(bool flag) {
	this->force_bidirectional_links = flag;	
	this->setReportedDesiredTxSlots(this->reported_desired_tx_slots);
}

void P2PLinkManager::setInitializeBidirectionalLinks() {
	if (this->reported_desired_tx_slots == 0)
		this->reported_desired_tx_slots = 1;
}

unsigned int P2PLinkManager::getNumUtilizedResources() const {
	if (current_link_state == nullptr)
		return 0;
	else 
		return current_link_state->burst_length;	
}