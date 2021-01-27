//
// Created by Sebastian Lindner on 06.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP

#include <vector>
#include <cstdint>
#include <Timestamp.hpp>
#include "Reservation.hpp"
#include "FrequencyChannel.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
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
			
			/**
			 * @param planning_horizon The number of time slots this reservation table will keep saved. It denotes the number of slots both into the future, as well as a history of as many slots. The number of slots saved is correspondingly planning_horizon*2.
			 */
			explicit ReservationTable(uint32_t planning_horizon);

			/**
			 * @param planning_horizon The number of time slots this reservation table will keep saved. It denotes the number of slots both into the future, as well as a history of as many slots. The number of slots saved is correspondingly planning_horizon*2.
			 * @param default_reservation The default reservation for new time slots (e.g. when update() is called).
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
			
			/**
			 * Attempts to find a number of candidate slots.
			 * @param min_offset The minimum offset in time for any candidate slot.
			 * @param num_candidates The number of candidate slots that should be found.
			 * @param range_length The number of slots that should be idle for each range that starts at the returned offsets.
			 * @param consider_transmitter Whether transmitter utilization should be taken into account, s.t. slots that are already marked for transmission are not included.
			 * @param consider_receivers Whether receiver utilization should be taken into account, s.t. slots where no receiver is available are not included.
			 * @return The start slots of each candidate. The size of the returned container should be checked to ensure that enough candidates were found.
			 */
			std::vector<int32_t> findCandidateSlots(unsigned int min_offset, unsigned int num_candidates, unsigned int range_length, bool consider_transmitter, bool consider_receivers) const;
			
			/**
			 * Locks the given slot_offsets in this reservation table so that they are not considered for later link establishment proposals' candidate slot_offsets.
			 * @param slot_offsets
			 * @param lock_tx Whether to lock the hardware transmitter, too.
			 * @param lock_rx Whether to lock a hardware receiver, too.
			 * @return Whether locking succeeded.
			 */
			bool lock(const std::vector<int32_t>& slot_offsets, bool lock_tx, bool lock_rx);
			
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
			 * @param start Slot offset that marks the earliest opportunity.
			 * @param length Number of slots of the slot range.
			 * @param consider_transmitter Whether a linked transmitter reservation table should be queried for being idle, too.
			 * @param consider_receivers Whether linked receiver reservation tables should be queried for being idle, too.
			 * @return Slot offset that marks the beginning of a completely idle slot range.
			 * @throws runtime_error If no suitable slot range can be found.
			 */
			int32_t findEarliestIdleRange(int32_t start, uint32_t length, bool consider_transmitter, bool consider_receivers) const;

			bool canLock(const std::vector<int32_t>& slot_offsets) const;
			
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
