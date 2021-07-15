//
// Created by Sebastian Lindner on 09.12.20.
//

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
		static constexpr unsigned int BITS_PER_SLOT = 8, BITS_PER_CHANNEL = 8, BITS_PER_ACTION = 3;

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
			auto &vec = local_reservations[center_freq];
			for (int t = 1; t < table->getPlanningHorizon(); t++) {
				const Reservation &res = table->getReservation(t);
				if (res.isBeaconTx() || res.isTx() || res.isTxCont())
					vec.push_back({t, res.getAction()});
			}
		}

		std::map<uint64_t, std::vector<std::pair<unsigned int, Reservation::Action>>> local_reservations;
	};

}
#endif //TUHH_INTAIRNET_MC_SOTDMA_BEACONPAYLOAD_HPP
