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

#ifndef TUHH_INTAIRNET_MC_SOTDMA_FREQUENCYCHANNEL_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_FREQUENCYCHANNEL_HPP

#include <cstdint>
#include <ostream>

namespace TUHH_INTAIRNET_MCSOTDMA {

	/** A logical frequency channel. */
	class FrequencyChannel {
	public:
		FrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth);
		FrequencyChannel(const FrequencyChannel &other);
		virtual ~FrequencyChannel() = default;

		uint64_t getCenterFrequency() const;

		uint64_t getBandwidth() const;

		/**		 
		 * @return Whether this is a point-to-point channel.
		 */
		bool isPP() const;

		/**		 
		 * @return Whether this is a shared channel.
		 */
		bool isSH() const;

		bool isBlocked() const;

		void setBlacklisted(bool value);

		bool operator==(const FrequencyChannel& other) const;

		bool operator!=(const FrequencyChannel& other) const;

		bool operator<(const FrequencyChannel& other) const;

		bool operator<=(const FrequencyChannel& other) const;

		bool operator>(const FrequencyChannel& other) const;

		bool operator>=(const FrequencyChannel& other) const;

	protected:
		/** Whether this is a point-to-point frequency channel for unicast communication. */
		const bool is_p2p;
		/** Center frequency in Hertz. */
		const uint64_t center_frequency;
		/** Bandwidth in Hertz. */
		const uint64_t bandwidth;
		/** FrequencyChannel object are local to each user, so they can blacklist a channel through this flag. */
		bool is_blacklisted;
	};

	inline std::ostream& operator<<(std::ostream& stream, const FrequencyChannel& channel) {
		if (channel.isPP())
			return stream << std::to_string(channel.getCenterFrequency()) << "kHz";
		else
			return stream << "SH";
	}
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_FREQUENCYCHANNEL_HPP
