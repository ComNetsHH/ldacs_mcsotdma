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

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BEACONPAYLOAD_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BEACONPAYLOAD_HPP

#include <L2Packet.hpp>
#include <MacId.hpp>
#include "FrequencyChannel.hpp"
#include "ReservationTable.hpp"
#include <map>

namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * Implements a beacon payload that encodes a user's reservations.
	 */
	class BeaconPayload : public L2Packet::Payload {
	public:
		static constexpr unsigned int BITS_PER_SLOT = 9, BITS_PER_CHANNEL = 9, BITS_PER_ACTION = 3;

		BeaconPayload() = default;

		/**
		 * Convenience constructor that calls `encode(table)` on each given table.
		 * @param reservation_tables
		 */
		explicit BeaconPayload(const std::vector<ReservationTable*>& reservation_tables) {
			for (const auto *table : reservation_tables) {
				if (table->getLinkedChannel() == nullptr)
					throw std::invalid_argument("BeaconPayload(rx_tables) got a ReservationTable with no linked FrequencyChannel.");
				this->encode(table->getLinkedChannel()->getCenterFrequency(), table);
			}
		}

		BeaconPayload(const BeaconPayload& other) : BeaconPayload() {
			for (const auto& item : other.local_reservations)
				for (const auto& pair : item.second)
					local_reservations[item.first].push_back(pair);
		}

		Payload* copy() const override {
			return new BeaconPayload(*this);
		}

		unsigned int getBits() const override {
			unsigned int bits = 0;
			for (auto pair : local_reservations) {
				bits += BITS_PER_CHANNEL;
				bits += pair.second.size() * (BITS_PER_SLOT + BITS_PER_ACTION);
			}
			return bits;
		}

		void encode(uint64_t center_freq, const ReservationTable *table) {			
			for (int t = 1; t < table->getPlanningHorizon(); t++) {
				const Reservation &res = table->getReservation(t);
				if (res.isBeaconTx() || res.isTx())
					local_reservations[center_freq].push_back({t, res.getAction()});
			}
		}

		std::map<uint64_t, std::vector<std::pair<unsigned int, Reservation::Action>>> local_reservations;
	};

}
#endif //TUHH_INTAIRNET_MC_SOTDMA_BEACONPAYLOAD_HPP
