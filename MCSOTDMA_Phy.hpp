//
// Created by Sebastian Lindner on 10.12.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_MCSOTDMA_PHY_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_MCSOTDMA_PHY_HPP

#include <IPhy.hpp>
#include "ReservationTable.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class MCSOTDMA_Phy : public IPhy {
		public:
			explicit MCSOTDMA_Phy(uint32_t planning_horizon);
			
			~MCSOTDMA_Phy() override;
			
			bool isTransmitterIdle(unsigned int slot_offset, unsigned int num_slots) const override;

			bool isAnyReceiverIdle(unsigned int slot_offset, unsigned int num_slots) const override;
			
			void update(uint64_t num_slots);
			
			ReservationTable* getTransmitterReservationTable();
			std::vector<ReservationTable*> getReceiverReservationTables();
		
		protected:
			/** Is notified by MAC ReservationTables of their reservations. */
			ReservationTable* transmitter_reservation_table = nullptr;
			std::vector<ReservationTable*> receiver_reservation_tables;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_MCSOTDMA_PHY_HPP
