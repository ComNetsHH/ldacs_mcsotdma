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

#ifndef TUHH_INTAIRNET_MC_SOTDMA_MCSOTDMA_PHY_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_MCSOTDMA_PHY_HPP

#include <IPhy.hpp>
#include <IOmnetPluggable.hpp>
#include <Statistic.hpp>
#include "ReservationTable.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class MCSOTDMA_Phy : public IPhy, public IOmnetPluggable {

		friend class MCSOTDMA_PhyTests;
		friend class MCSOTDMA_MacTests;
		friend class SystemTests;
		friend class ManyUsersTests;

	public:
		explicit MCSOTDMA_Phy(uint32_t planning_horizon);

		~MCSOTDMA_Phy() override;

		bool isTransmitterIdle(unsigned int slot_offset, unsigned int num_slots) const override;

		bool isAnyReceiverIdle(unsigned int slot_offset, unsigned int num_slots) const override;

		void update(uint64_t num_slots) override;

		ReservationTable* getTransmitterReservationTable();

		std::vector<ReservationTable*>& getReceiverReservationTables();

		void onReception(L2Packet* packet, uint64_t center_frequency) override;

	protected:
		/** Is notified by MAC ReservationTables of their reservations. */
		ReservationTable* transmitter_reservation_table = nullptr;
		std::vector<ReservationTable*> receiver_reservation_tables;
		Statistic stat_num_packets_rcvd = Statistic("phy_statistic_num_packets_received", this);
		/** Collects the number of packets intended for this user that were missed because no receiver was tuned to the channel. */
		Statistic stat_num_packets_missed = Statistic("phy_statistic_num_packets_missed", this);
		std::vector<Statistic*> statistics = {
				&stat_num_packets_rcvd,
				&stat_num_packets_missed
		};
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_MCSOTDMA_PHY_HPP
