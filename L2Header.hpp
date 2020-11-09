//
// Created by Sebastian Lindner on 04.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_L2HEADER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_L2HEADER_HPP

#include <algorithm>
#include "LinkId.hpp"

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
			L2HeaderBase(const LinkId& icao_id, unsigned int offset, unsigned short length_current, unsigned short length_next, unsigned int timeout)
			: L2Header(), icao_id(icao_id), offset(offset), length_current(length_current), length_next(length_next), timeout(timeout) {
				this->frame_type = FrameType::base;
				if (icao_id == LINK_ID_UNSET)
					throw std::invalid_argument("Cannot instantiate a header with an unset ICAO ID.");
			}
			
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
			
			const LinkId& getId() const {
				return this->icao_id;
			}
		
		protected:
			/** Source ID. */
			const LinkId icao_id;
	};
	
	class L2HeaderBroadcast : public L2Header {
		public:
			L2HeaderBroadcast()
			: L2Header() {
				this->frame_type = FrameType::broadcast;
			}
			
			unsigned int getBits() const override {
				return L2Header::getBits();
			}
	};
	
//	class L2HeaderBeacon : public L2Header {
//		public:
//			L2HeaderBeacon()
//	};
	
	class L2HeaderUnicast : public L2Header {
		public:
			L2HeaderUnicast(const LinkId& icao_dest_id, bool use_arq, unsigned int arq_seqno, unsigned int arq_ack_no, unsigned int arq_ack_slot)
			: L2Header(), icao_dest_id(icao_dest_id), use_arq(use_arq), arq_seqno(arq_seqno), arq_ack_no(arq_ack_no), arq_ack_slot(arq_ack_slot) {
				this->frame_type = FrameType::unicast;
				if (icao_dest_id == LINK_ID_UNSET)
					throw std::invalid_argument("Cannot instantiate a header with an unset ICAO ID.");
			}
			
			const LinkId& getDestId() const {
				return this->icao_dest_id;
			}
			
			unsigned int getBits() const override {
				return 1 /* Whether ARQ is used */
				+ 8 /* ARQ sequence number */
				+ 8 /* ARQ ACK number */
				+ 8 /* ARQ slot indication number */
				+ icao_dest_id.getBits() /* destination ID */
				+ L2Header::getBits();
			}
			
			/** Whether the ARQ protocol is followed for this transmission, i.e. acknowledgements are expected. */
			bool use_arq;
			/** ARQ sequence number. */
			unsigned int arq_seqno;
			/** ARQ acknowledgement. */
			unsigned int arq_ack_no;
			/** The offset to the next reserved slot where an acknowledgement is expected. */
			unsigned int arq_ack_slot;
		
		protected:
			/** Destination ICAO ID. */
			const LinkId icao_dest_id;
	};
	
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_L2HEADER_HPP
