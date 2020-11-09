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
				unset,
				base,
				beacon,
				broadcast,
				unicast,
				link_establishment_request
			};
			
			explicit L2Header()	: frame_type(FrameType::unset), crc_checksum(0) {}
			
			virtual unsigned int getBits() const {
				return 3 /* frame type */
				       + 16 /* CRC */;
			}
			
			/** This frame's type. */
			FrameType frame_type;
			/** CRC checksum. */
			unsigned int crc_checksum;
	};
	
	class L2HeaderBase : public L2Header {
		public:
			L2HeaderBase(IcaoId icao_id, unsigned int offset, unsigned short length_current, unsigned short length_next, unsigned int timeout)
			: L2Header(), icao_id(std::move(icao_id)), offset(offset), length_current(length_current), length_next(length_next), timeout(timeout) {
				this->frame_type = FrameType::base;
			}
			
			/** Source ID. */
			IcaoId icao_id;
			/** Number of slots until this reservation is next transmitted. */
			unsigned int offset;
			/** Number of slots this frame occupies. */
			unsigned short length_current;
			/** Number of slots next frame will occupy. */
			unsigned short length_next;
			/** Remaining number of repetitions this reservation remains valid for. */
			unsigned int timeout;
			
			unsigned int getBits() const override {
				return icao_id.getBits()
					   + 8 /* offset */
					   + 4 /* length_current */
					   + 4 /* length_next */
				       + 8 /* timeout */
				       + L2Header::getBits();
			}
	};
	
	class L2HeaderBroadcast : public L2HeaderBase {
		public:
			L2HeaderBroadcast(IcaoId icao_id, unsigned int offset, unsigned short length_current, unsigned short length_next, unsigned int timeout)
			: L2HeaderBase(std::move(icao_id), offset, length_current, length_next, timeout) {
				this->frame_type = FrameType::broadcast;
			}
	};
	
	class L2HeaderUnicast : public L2HeaderBase {
		public:
			L2HeaderUnicast(IcaoId icao_src_id, IcaoId icao_dest_id, unsigned int offset, unsigned short length_current, unsigned short length_next, unsigned int timeout, bool use_arq, unsigned int arq_seqno, unsigned int arq_ack_no, unsigned int arq_ack_slot)
			: L2HeaderBase(std::move(icao_src_id), offset, length_current, length_next, timeout), icao_dest_id(std::move(icao_dest_id)), use_arq(use_arq), arq_seqno(arq_seqno), arq_ack_no(arq_ack_no), arq_ack_slot(arq_ack_slot) {
				this->frame_type = FrameType::unicast;
			}
			
			/** Destination ICAO ID. */
			IcaoId icao_dest_id;
			/** Whether the ARQ protocol is followed for this transmission, i.e. acknowledgements are expected. */
			bool use_arq;
			/** ARQ sequence number. */
			unsigned int arq_seqno;
			/** ARQ acknowledgement. */
			unsigned int arq_ack_no;
			/** The offset to the next reserved slot where an acknowledgement is expected. */
			unsigned int arq_ack_slot;
	};
	
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_L2HEADER_HPP
