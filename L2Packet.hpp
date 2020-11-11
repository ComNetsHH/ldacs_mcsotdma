#include <utility>
#include <vector>

//
// Created by Sebastian Lindner on 06.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_L2PACKET_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_L2PACKET_HPP

#include "L2Header.hpp"
#include <iostream>
#include <string>

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * Wraps around an original packet implementation.
	 * It keeps a pointer to the original packet and adds functionality specific to the MC-SOTDMA protocol.
	 * When MC-SOTDMA operation finishes, the original packet is passed on to the respective receiving layer.
	 */
	class L2Packet {
		public:
			/**
			 * Interface for a wrapper of around an upper-layer packet.
		    */
			class Payload {
				public:
					virtual unsigned int getBits() const = 0;
			};
			
			explicit L2Packet() : headers(), payloads(), dest_id(SYMBOLIC_ID_UNSET) {}
			
			void addPayload(L2Header* header, Payload* payload) {
				// Ensure that the first header is a base header.
				if (headers.empty() && header->frame_type != L2Header::FrameType::base)
					throw std::invalid_argument("First header of a packet *must* be a base header.");
				
				// Ensure that later headers are *not* base headers.
				if (!headers.empty() && header->frame_type == L2Header::FrameType::base)
					throw std::invalid_argument("Later headers of a packet cannot be a base header.");
				
				// Set the unicast destination ID if possible.
				if (header->frame_type == L2Header::FrameType::unicast) {
					IcaoId header_dest_id = ((L2HeaderUnicast*) header)->getDestId();
					// Sanity check that the destination ID is actually set.
					if (header_dest_id == SYMBOLIC_ID_UNSET)
						throw std::runtime_error("Cannot add a unicast header with an unset destination ID.");
					// If currently there's no set destination, we set it now.
					if (this->dest_id == SYMBOLIC_ID_UNSET)
						this->dest_id = header_dest_id;
					// If there is a set non-broadcast destination, it must be unicast.
					// So if these differ, throw an error.
					else if (this->dest_id != SYMBOLIC_LINK_ID_BROADCAST && this->dest_id != SYMBOLIC_LINK_ID_BEACON && header_dest_id != this->dest_id)
						throw std::runtime_error("Cannot add a unicast header to this packet. It already has a unicast destination ID. Current dest='" + std::to_string(this->dest_id.getId()) + "' header dest='" + std::to_string(header_dest_id.getId()) + "'.");
				}
				
				// Set the broadcast destination ID if possible.
				if (header->frame_type == L2Header::FrameType::broadcast) {
					// If currently there's no set destination, we set it now.
					if (this->dest_id == SYMBOLIC_ID_UNSET)
						this->dest_id = SYMBOLIC_LINK_ID_BROADCAST;
					// If there already is a set destination, it may only be a beacon.
					else if (this->dest_id != SYMBOLIC_LINK_ID_BEACON)
						throw std::runtime_error("Cannot add a broadcast header to this packet. It already has a destination ID: '" + std::to_string(this->dest_id.getId()) + "'.");
				}
				
				// Set the beacon destination ID if possible.
				if (header->frame_type == L2Header::FrameType::beacon) {
					// If currently there's no set destination, we set it now.
					if (this->dest_id == SYMBOLIC_ID_UNSET)
						this->dest_id = SYMBOLIC_LINK_ID_BEACON;
					// If there already is a set destination, throw an error, as beacon headers must come first.
					else
						throw std::runtime_error("Cannot add a beacon header to this packet. It already has a destination ID: '" + std::to_string(this->dest_id.getId()) + "'.");
				}
				
				headers.push_back(header);
				payloads.push_back(payload);
			}
			
			/**
			 * @return All payloads.
			 */
			const std::vector<Payload*>& getPayloads() {
				return this->payloads;
			}
			
			/**
			 * @return All headers.
			 */
			const std::vector<L2Header*>& getHeaders() {
				return this->headers;
			}
			
			/**
			 * @return Total size of this packet in bits, consisting of both headers and payloads.
			 */
			unsigned int getBits() const {
				unsigned int bits = 0;
				for (size_t i = 0; i < headers.size(); i++)
					bits += headers.at(i)->getBits() + payloads.at(i)->getBits();
				return bits;
			}
			
			/**
			 * @return This packet's destination ID.
			 */
			const IcaoId& getDestination() const {
				return this->dest_id;
			}
			
		protected:
			/**
			 * Several headers can be concatenated to fill one packet.
			 */
			std::vector<L2Header*> headers;
			
			/**
			 * Several payloads can be concatenated (with resp. headers) to fill one packet.
			 */
			std::vector<Payload*> payloads;
			
			IcaoId dest_id;
		
		protected:
			/**
			 * Ensures that at least one header is present, which must be a base header.
			 * @throws std::logic_error If no headers are present.
			 * @throws std::runtime_error If first header is not a base header.
			 */
			void validateHeader() const {
				if (headers.empty())
					throw std::logic_error("No headers present.");
				const L2Header* first_header = headers.at(0);
				if (first_header->frame_type != L2Header::base)
					throw std::runtime_error("First header is not a base header.");
			}
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_L2PACKET_HPP
