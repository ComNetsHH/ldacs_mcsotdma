//
// Created by Sebastian Lindner on 06.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP

#include <vector>
#include <cstdint>

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * A reservation table keeps track of all slots of a particular, logical frequency channel for a pre-defined planning horizon.
	 * It is regularly updated when new information about slot utilization becomes available.
	 * It can be queried to find ranges of idle slots suitable for communication.
	 */
	class ReservationTable {
		public:
			friend class ReservationTableTests;
			
			/**
			 * @param planning_horizon The number of time slots this reservation table will keep saved. It denotes the number of slots both into the future, as well as a history of as many slots. The number of slots saved is correspondingly planning_horizon*2.
			 */
			explicit ReservationTable(uint32_t planning_horizon);
			
			/**
			 * @param slot_offset Offset to the current slot. Positive values for future slots, zero for the current slot and negative values for past slots.
			 * @return Whether the specified slot is marked as idle.
			 */
			bool isIdle(int32_t slot_offset);
			
			/**
			 * @param slot_offset Offset to the current slot. Positive values for future slots, zero for the current slot and negative values for past slots.
			 * @return Whether the specified slot is marked as utilized.
			 */
			bool isUtilized(int32_t slot_offset);
			
			/**
			 * @param start Slot offset that marks the beginning of the range of slots.
			 * @param length Number of slots in the range.
			 * @return Whether the specified slot range is fully idle.
			 */
			bool isIdle(int32_t start, uint32_t length);
			
			/**
			 * @param start Slot offset that marks the beginning of the range of slots.
			 * @param length Number of slots in the range.
			 * @return Whether the specified slot range is utilized.
			 */
			bool isUtilized(int32_t start, uint32_t length);
			
			/**
			 * @return Number of slots this table keeps values for in either direction of time.
			 */
			uint32_t getPlanningHorizon() const;
			
			/**
			 * Marks the slot at 'offset' as either utilized or idle.
			 * @param slot_offset
			 * @param utilized
			 */
			void mark(int32_t slot_offset, bool utilized);
			
			/**
			 * @param start Slot offset that marks the earliest opportunity.
			 * @param length Number of slots of the slot range.
			 * @return Slot offset that marks the beginning of a completely idle slot range.
			 * @throws runtime_error If no suitable slot range can be found.
			 */
			int32_t findEarliestIdleRange(int32_t start, uint32_t length);
			
			/**
			 * @return The discrete 64-bit number that denotes what this reservation table assumes as the current moment in time, which is the last time it was updated.
			 */
			uint64_t getCurrentSlot() const;
			
			/**
			 * Progress time for this reservation table. Old values are dropped, new values are added.
			 * @param num_slots The number of slots that have passed since the last update.
			 */
			void update(uint64_t num_slots);
			
			/**
			 * @return Reference to the internally-used slot utilization vector.
			 */
			const std::vector<bool>& getVec() const;
		
		protected:
			bool isValid(int32_t slot_offset) const;
			
			bool isValid(int32_t start, uint32_t length) const;
			
			/**
			 * The slot_utilization_vec keeps both historic, current and future values.
			 * A logical signed integer can represent past values through negative, current through zero, and future through positive values.
			 * @param slot_offset
			 * @return The index that can be used to address the corresponding value indicated by the logical offset.
			 */
			uint64_t convertOffsetToIndex(int32_t slot_offset) const;
			
		protected:
			/** Holds the utilization status of every slot from the current one up to some planning horizon both into past and future. */
			std::vector<bool> slot_utilization_vec;
			/** Specifies the number of slots this reservation table holds values for both into the future and into the past. In total, twice the planning horizon is covered. */
			const uint32_t planning_horizon;
			/** OMNeT++ has discrete points in time that can be represented by a 64-bit number. This keeps track of that moment in time where this table was last updated. */
			uint64_t last_updated;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP
