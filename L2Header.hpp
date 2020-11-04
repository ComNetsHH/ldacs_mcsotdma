//
// Created by Sebastian Lindner on 04.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_L2HEADER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_L2HEADER_HPP

#include <algorithm>
#include "IcaoId.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * Specifies the MC-SOTDMA layer-2 header.
	 */
	class L2Header {
		public:
			enum FrameType {
				beacon,
				broadcast,
				link_establishment_request
			};
			
			L2Header(IcaoId icao_id, unsigned int timeout, FrameType frame_type, unsigned int crc_checksum)
				: icao_id(std::move(icao_id)), timeout(timeout), frame_type(frame_type), crc_checksum(crc_checksum) {}
			
			IcaoId icao_id;
			unsigned int timeout;
			FrameType frame_type;
			unsigned int crc_checksum;
			
			unsigned int getBits() const {
				return icao_id.getBits()
					+ 8 /* timeout */
					+ 3 /* frame type */
					+ 16 /* CRC */;
			}
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_L2HEADER_HPP
