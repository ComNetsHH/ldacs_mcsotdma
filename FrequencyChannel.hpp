//
// Created by Sebastian Lindner on 14.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_FREQUENCYCHANNEL_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_FREQUENCYCHANNEL_HPP

#include <cstdint>

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/** A logical frequency channel. */
	class FrequencyChannel {
		public:
			FrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth);
			
			uint64_t getCenterFrequency() const;
			
			uint64_t getBandwidth() const;
			
			bool isPointToPointChannel() const;
			
			bool isBroadcastChannel() const;
			
			bool isBlacklisted() const;
			
			void setBlacklisted(bool value);
			
			bool operator==(const FrequencyChannel& other) const;
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
	
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_FREQUENCYCHANNEL_HPP
