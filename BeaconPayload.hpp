//
// Created by Sebastian Lindner on 09.12.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BEACONPAYLOAD_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BEACONPAYLOAD_HPP

#include <L2Packet.hpp>
#include <MacId.hpp>
#include "FrequencyChannel.hpp"
#include "ReservationTable.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * Implements a beacon payload that encodes a user's reservations.
	 */
	class BeaconPayload : public L2Packet::Payload {
	public:
		static constexpr unsigned int BITS_PER_SLOT = 8, BITS_PER_CHANNEL = 8;

		BeaconPayload() = default;

		BeaconPayload(const BeaconPayload& other) : BeaconPayload() {
			for (const auto& pair : other.local_reservations)
				for (unsigned int t : pair.second)
					local_reservations.at(pair.first).push_back(t);
		}

		Payload* copy() const override {
			return new BeaconPayload(*this);
		}

		unsigned int getBits() const override {
			unsigned int bits = 0;
			for (auto pair : local_reservations) {
				bits += BITS_PER_CHANNEL;
				bits += pair.second.size() * BITS_PER_SLOT;
			}
			return bits;
		}

		void encode(const ReservationTable *table) {
			FrequencyChannel channel = FrequencyChannel(*table->getLinkedChannel());
			for (int t = 0; t < table->getPlanningHorizon(); t++) {
				const Reservation &res = table->getReservation(t);
				if (res.isBeacon() || res.isTx() || res.isTxCont() || res.isRx() || res.isRxCont())
					local_reservations.at(channel).push_back(t);
			}
		}

		std::map<FrequencyChannel, std::vector<unsigned int>> local_reservations;
	};

}
#endif //TUHH_INTAIRNET_MC_SOTDMA_BEACONPAYLOAD_HPP
