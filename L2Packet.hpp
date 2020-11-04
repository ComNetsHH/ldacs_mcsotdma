#include <utility>
#include <vector>

//
// Created by Sebastian Lindner on 06.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_L2PACKET_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_L2PACKET_HPP

#include "L2Header.hpp"

/**
 * Interface for a wrapper of around an upper-layer packet.
 */
class Packet {
	public:
		virtual unsigned int getBits() const = 0;
};

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * Wraps around an original packet implementation.
	 * It keeps a pointer to the original packet and adds functionality specific to the MC-SOTDMA protocol.
	 * When MC-SOTDMA operation finishes, the original packet is passed on to the respective receiving layer.
	 */
	class L2Packet {
		public:
			explicit L2Packet() : headers(), payloads() {}
			
			void addPayload(L2Header header, Packet* payload) {
				headers.push_back(header);
				payloads.push_back(payload);
			}
			
			void removePayload(size_t index) {
				headers.erase(headers.begin() + index);
				payloads.erase(payloads.begin() + index);
			}
			
			/**
			 * @return All payloads.
			 */
			const std::vector<Packet*>& getPayloads() {
				return this->payloads;
			}
			
			/**
			 * @return All headers.
			 */
			const std::vector<L2Header>& getHeaders() {
				return this->headers;
			}
			
			/**
			 * @return Total size of this packet in bits, consisting of both headers and payloads.
			 */
			unsigned int getBits() const {
				unsigned int bits = 0;
				for (size_t i = 0; i < headers.size(); i++)
					bits += headers.at(i).getBits() + payloads.at(i)->getBits();
				return bits;
			}
			
		protected:
			/**
			 * Several headers can be concatenated to fill one packet.
			 */
			std::vector<L2Header> headers;
			
			/**
			 * Several payloads can be concatenated (with resp. headers) to fill one packet.
			 */
			std::vector<Packet*> payloads;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_L2PACKET_HPP
