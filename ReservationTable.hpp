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

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP

#include <vector>
#include <cstdint>
#include <Timestamp.hpp>
#include "Reservation.hpp"
#include "FrequencyChannel.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class MCSOTDMA_Mac;

	class no_rx_available_error : public std::runtime_error {
	public:
		explicit no_rx_available_error(const std::string& arg) : std::runtime_error(arg) {}
	};

	class no_tx_available_error : public std::runtime_error {
	public:
		explicit no_tx_available_error(const std::string& arg) : std::runtime_error(arg) {}
	};

	class id_mismatch : public std::invalid_argument {
	public:
		explicit id_mismatch(const std::string &arg) : std::invalid_argument(arg) {}
	};

	class cannot_lock : public std::invalid_argument {
	public:
		explicit cannot_lock(const std::string &arg) : std::invalid_argument(arg) {}
	};

	/**
	 * A reservation table keeps track of all slots of a particular, logical frequency channel for a pre-defined planning horizon.
	 * It is regularly updated when new information about slot utilization becomes available.
	 * It can be queried to find ranges of idle slots suitable for communication.
	 */
	class ReservationTable {
	public:
		friend class ReservationTableTests;

		friend class LinkManagementEntity;

		friend class LinkManagerTests;

		friend class LinkManagementEntityTests;

		ReservationTable();

		/**
		 * @param planning_horizon The number of time slots this reservation table will keep saved. It denotes the number of slots both into the future, as well as a history of as many slots. The number of slots saved is correspondingly planning_horizon*2.
		 */
		explicit ReservationTable(uint32_t planning_horizon);

		/**
		 * @param planning_horizon The number of time slots this reservation table will keep saved. It denotes the number of slots both into the future, as well as a history of as many slots. The number of slots saved is correspondingly planning_horizon*2.
		 * @param default_reservation The default reservation for new time slots (e.g. when onSlotStart() is called).
		 */
		ReservationTable(uint32_t planning_horizon, const Reservation& default_reservation);

		virtual ~ReservationTable();

		/**
		 * Marks the slot at 'offset' with a reservation.
		 * @param slot_offset
		 * @param reservation
		 * @throws std::invalid_argument If linked transmitter or receiver tables have no capacity for a corresponding TX/RX reservation.
		 */
		Reservation* mark(int32_t slot_offset, const Reservation& reservation);

		/**
		 * Progress time for this reservation table. Old values are dropped, new values are added.
		 * Also increments the last_updated timestamp by by num_slots.
		 * @param num_slots The number of slots that have passed since the last hasControlMessage.
		 */
		void update(uint64_t num_slots);

		/**
		 * @return The number of *future* slots that are marked as idle.
		 */
		uint64_t getNumIdleSlots() const;

		/**
		 * @return The last time this table was updated.
		 */
		const Timestamp& getCurrentSlot() const;

		/**
		 * @param offset
		 * @return The reservation at the specified offset.
		 */
		const Reservation& getReservation(int offset) const;		

		std::vector<unsigned int> findSHCandidates(unsigned int num_candidates, int min_offset) const;

		/**		 
		 * @param num_proposal_slots 
		 * @param min_offset 
		 * @param burst_offset 
		 * @param burst_length 
		 * @param burst_length_tx 
		 * @param timeout 		 
		 * @return Start slot offsets that could be used to initiate a PP link.
		 */
		std::vector<unsigned int> findPPCandidates(unsigned int num_proposal_slots, unsigned int min_offset, int num_forward_bursts, int num_reverse_bursts, int period, int timeout) const;		

		/**		 
		 * @param slot_offset 
		 * @param id 
		 * @throws std::invalid_argument if reservation at slot_offset is not locked 
		 * @throws id_mismatch if ID does not match
		 * @returns whether locking succeeded
		 */
		bool lock(unsigned int slot_offset, const MacId& id);

		/**
		 * When processing third-party link requests, one resource may be locked for id1's transmission or id2's transmission, as several proposals are made.
		 * This function attempts to lock the resource to the first ID. If it is already locked to the second ID, no error is thrown.
		 * Through this, the indicated ID may not belong to the correct ID after link establishment, but when the link reply is processed, this is corrected.
		 * @param slot_offset 
		 * @param id1 
		 * @param id2 
		 * @returns whether locking succeeded
		 * @throws std::invalid_argument if reservation at slot_offset is not locked 
		 * @throws id_mismatch if neither ID matches		 
		 */
		bool lock_either_id(unsigned int slot_offset, const MacId& id1, const MacId& id2);

		/**		 
		 * @param slot_offset 
		 * @param id 
		 * @throws std::invalid_argument if reservation at slot_offset is not locked 
		 * @throws id_mismatch if ID does not match
		 */
		void unlock(unsigned int slot_offset, const MacId& id);

		/** 
		 * @param slot_offset 
		 * @param id1 
		 * @param id2 
		 * @throws std::invalid_argument if reservation at slot_offset is not locked 
		 * @throws id_mismatch if neither ID matches
		 */
		void unlock_either_id(unsigned int slot_offset, const MacId& id1, const MacId& id2);

		/**
		 * @param start_offset The minimum slot offset to start the search.
		 * @param reservation
		 * @return The slot offset until the earliest reservation that corresponds to the one provided.
		 * @throws std::runtime_error If no reservation of this kind is found.
		 */
		int32_t findEarliestOffset(int32_t start_offset, const Reservation& reservation) const;

		void linkFrequencyChannel(FrequencyChannel* channel);

		const FrequencyChannel* getLinkedChannel() const;

		/**
		 * @param slot_offset Offset to the current slot. Positive values for future slots, zero for the current slot and negative values for past slots.
		 * @return Whether the specified slot is marked as idle.
		 */
		bool isIdle(int32_t slot_offset) const;

		bool isLocked(int32_t slot_offset) const;

		/**
		 * @param slot_offset Offset to the current slot. Positive values for future slots, zero for the current slot and negative values for past slots.
		 * @return Whether the specified slot is marked as utilized.
		 */
		bool isUtilized(int32_t slot_offset) const;

		bool anyTxReservations(int32_t slot_offset) const;

		bool anyTxReservations(int32_t start, uint32_t length) const;

		bool anyRxReservations(int32_t slot_offset) const;

		bool anyRxReservations(int32_t start, uint32_t length) const;

		/**
		 * @param start Slot offset that marks the beginning of the range of slots.
		 * @param length Number of slots in the range.
		 * @return Whether the specified slot range is fully idle.
		 */
		bool isIdle(int32_t start, uint32_t length) const;

		/**
		 * @param start Slot offset that marks the beginning of the range of slots.
		 * @param length Number of slots in the range.
		 * @return Whether the specified slot range is utilized.
		 */
		bool isUtilized(int32_t start, uint32_t length) const;

		/**
		 * @param id
		 * @return The number of TX or TX_CONT reservations that belong to the user with 'id'.
		 */
		unsigned long countReservedTxSlots(const MacId& id) const;

		/**
		 * @param id
		 * @return A new ReservationTable that contains all TX and TX_CONT reservations targeted at 'id'.
		 */
		ReservationTable* getTxReservations(const MacId& id) const;

		/**
		 * @return Number of slots this table keeps values for in either direction of time.
		 */
		uint32_t getPlanningHorizon() const;

		/**
		 * Copies all TX and TX_CONT reservations from 'other' into this ReservationTable.
		 * @param other
		 */
		void integrateTxReservations(const ReservationTable* other);

		/**
		 * @param other
		 * @return True if all reservations match.
		 */
		bool operator==(const ReservationTable& other) const;

		/**
		 * @param other
		 * @return False if all reservations match.
		 */
		bool operator!=(const ReservationTable& other) const;

		/**
		 * Links a transmitter reservation table.
		 * @param tx_table
		 */
		void linkTransmitterReservationTable(ReservationTable* tx_table);

		/**
		 * Links a receiver reservation table.
		 * @param rx_table
		 */
		void linkReceiverReservationTable(ReservationTable* rx_table);

		bool canLock(unsigned int slot_offset) const;		

		bool isLinkValid(int start_slot_offset, int period, int num_tx_initiator, int num_tx_recipient, int timeout, bool is_link_initiator) const;

	protected:
		bool isValid(int32_t slot_offset) const;

		bool isValid(int32_t start, uint32_t length) const;

		/**
		 * Sets what this table regards as the current moment in time.
		 * Should be used just once at the start - calls to 'hasControlMessage()' will increment the timestamp henceforth.
		 * @param timestamp
		 */
		void setLastUpdated(const Timestamp& timestamp);

		/**
		 * @return Reference to the internally-used slot utilization vector.
		 */
		const std::vector<Reservation>& getVec() const;

		/**
		 * The slot_utilization_vec keeps both historic, current and future values.
		 * A logical signed integer can represent past values through negative, current through zero, and future through positive values.
		 * @param slot_offset
		 * @return The index that can be used to address the corresponding value indicated by the logical offset.
		 */
		uint64_t convertOffsetToIndex(int32_t slot_offset) const;		

		/**		 
		 * @param start_offset 
		 * @param burst_length 
		 * @param burst_length_tx 
		 * @param burst_offset 
		 * @param timeout 
		 * @return Start slot where a link with the given characteristics can be initiated.
		 */
		unsigned int findEarliestIdleSlotsPP(int start_offset, int num_forward_bursts, int num_reverse_bursts, int period, int timeout) const;
		unsigned int findEarliestIdleSlotsBC(unsigned int start_offset) const;

		/**
		 * @param start_slot
		 * @param burst_length
		 * @param burst_length_tx
		 * @param rx_idle_during_first_slot
		 * @return Whether the given transmission burst is reservable.
		 */
		bool isBurstValid(int start_slot, unsigned int burst_length, unsigned int burst_length_tx, bool rx_idle_during_first_slot, MCSOTDMA_Mac *mac) const;
		bool isTxValid(int slot) const;
		bool isRxValid(int slot) const;

	protected:
		/** Holds the utilization status of every slot from the current one up to some planning horizon both into past and future. */
		std::vector<Reservation> slot_utilization_vec;
		/** Specifies the number of slots this reservation table holds values for both into the future and into the past. In total, twice the planning horizon is covered. */
		const uint32_t planning_horizon;
		/** OMNeT++ has discrete points in time that can be represented by a 64-bit number. This keeps track of that moment in time where this table was last updated. */
		Timestamp last_updated;
		/** The ReservationTable keeps track of the idle slots it currently has, so that different tables are easily compared for their capacity of new reservations. */
		uint64_t num_idle_future_slots;
		FrequencyChannel* freq_channel = nullptr;

		/** The ReservationTable of the single transmitter may be linked, so that all TX reservations are forwarded to it. */
		ReservationTable* transmitter_reservation_table = nullptr;

		/** The ReservationTables of any receiver may be linked, so that all RX reservations can be forwarded to them. */
		std::vector<ReservationTable*> receiver_reservation_tables;
		Reservation default_reservation;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP
