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

		explicit BeaconPayload(const MacId& beacon_owner_id) : beacon_owner_id(beacon_owner_id) {}

		BeaconPayload(const BeaconPayload& other) : BeaconPayload(other.beacon_owner_id) {
			for (const auto& item : other.local_reservations)
				local_reservations.push_back(item);
		}

		Payload* copy() const override {
			return new BeaconPayload(*this);
		}

		~BeaconPayload() override {
			for (const auto& pair : local_reservations)
				delete pair.second;
		}

		unsigned int getBits() const override {
			unsigned int bits = 0;
			for (auto pair : local_reservations) {
				bits += pair.second->countReservedTxSlots(beacon_owner_id) * BITS_PER_SLOT;
				bits += BITS_PER_CHANNEL;
			}
			return bits;
		}

		std::vector<std::pair<FrequencyChannel, ReservationTable*>> local_reservations;
		const MacId beacon_owner_id;
	};

}
#endif //TUHH_INTAIRNET_MC_SOTDMA_BEACONPAYLOAD_HPP
