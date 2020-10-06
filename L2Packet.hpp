//
// Created by Sebastian Lindner on 06.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_L2PACKET_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_L2PACKET_HPP

class Packet;

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * Wraps around an original packet implementation.
	 * It keeps a pointer to the original packet and adds functionality specific to the MC-SOTDMA protocol.
	 * When MC-SOTDMA operation finishes, the original packet is passed on to the respective receiving layer.
	 */
	class L2Packet {
		public:
			explicit L2Packet(Packet* packet) : packet(packet) {}
			
			/**
			 * @return Encapulated, original packet.
			 */
			Packet* getPacket() {
				return this->packet;
			}
			
		protected:
			Packet* packet = nullptr;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_L2PACKET_HPP
