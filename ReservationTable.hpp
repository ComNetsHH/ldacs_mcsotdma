//
// Created by Sebastian Lindner on 06.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP

#include <vector>
#include <cstdint>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class ReservationTable {
		public:
			explicit ReservationTable(uint32_t planning_horizon);
			
			/**
			 * @param slot_offset Number of slots later than the current slot.
			 * @return Whether the specified slot is marked as utilized.
			 */
			bool isIdle(uint32_t slot_offset);
			
			/**
			 * @param start Slot offset that marks the beginning of the range of slots.
			 * @param end Slot offset that marks the (eclusive) end of the range of slots.
			 * @return Slot status of the specified slot range.
			 */
			std::vector<bool> isIdle(uint32_t start, uint32_t end);
			
			/**
			 * @return Number of slots this table keeps values for.
			 */
			uint32_t getPlanningHorizon() const;
			
			/**
			 * Marks the slot at 'offset' as either utilized or idle.
			 * @param slot_offset
			 * @param utilized
			 */
			void mark(uint32_t slot_offset, bool utilized);
			
			/**
			 * @param start Slot offset that marks the earliest opportunity.
			 * @param length Number of slots of the slot range.
			 * @return Slot offset that marks the beginning of a completely idle slot range.
			 */
			uint32_t findEarliestIdleRange(uint32_t start, uint32_t length);
			
		protected:
			/** Holds the utilization status of every slot from the current one up to some planning horizon. */
			std::vector<bool> slot_utilization_vec;
			/** Specifies the number of slots this reservation table holds values for. */
			uint32_t planning_horizon;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONTABLE_HPP
