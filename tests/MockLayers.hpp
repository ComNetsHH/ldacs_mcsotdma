//
// Created by Sebastian Lindner on 07.12.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_MOCKLAYERS_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_MOCKLAYERS_HPP

#include <IPhy.hpp>
#include <IArq.hpp>
#include <IRlc.hpp>
#include <INet.hpp>
#include "../coutdebug.hpp"
#include "../ReservationManager.hpp"
#include "../MCSOTDMA_Mac.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class PHYLayer : public IPhy {
		public:
			void receiveFromUpper(L2Packet* data, unsigned int center_frequency) override {
				if (data == nullptr)
					throw std::invalid_argument("PHY::receiveFromUpper(nullptr)");
				coutd << "PHY::receiveFromUpper(" << data->getBits() << "bits, " << center_frequency << "kHz)" << std::endl;
				outgoing_packets.push_back(data);
			}
			
			unsigned long getCurrentDatarate() const override {
				return 1600; // 200B/slot
			}
			
			~PHYLayer() override {
				for (L2Packet* packet : outgoing_packets)
					delete packet;
			}
			
			std::vector<L2Packet*> outgoing_packets;
	};
	
	class MACLayer : public MCSOTDMA_Mac {
		public:
			explicit MACLayer(const MacId& id, ReservationManager* manager) : MCSOTDMA_Mac(id, manager) {}
			
			CPRPosition getPosition() const override {
				return CPRPosition(10, 11, 12);
			}
		
		protected:
			void onReceptionSlot(const FrequencyChannel* channel) override {
				// do nothing.
			}
	};
	
	class ARQLayer : public IArq {
		public:
			void notifyOutgoing(unsigned int num_bits, const MacId& mac_id) override {
				coutd << "ARQ::notifyOutgoing(" << num_bits << ", " << mac_id.getId() << ")" << std::endl;
			}
			
			L2Packet* requestSegment(unsigned int num_bits, const MacId& mac_id) override {
				coutd << "ARQ::requestSegment... ";
				return upper_layer->requestSegment(num_bits, mac_id);
			}
			
			bool shouldLinkBeArqProtected(const MacId& mac_id) const override {
				return false;
			}
			
			void notifyAboutNewLink(const MacId& id) override {
			
			}
			
			void notifyAboutRemovedLink(const MacId& id) override {
			
			}
		
		protected:
			void processIncomingHeader(L2Packet* incoming_packet) override {
			
			}
	};
	
	class RLCLayer : public IRlc {
		public:
			
			class RLCPayload : public L2Packet::Payload {
				public:
					explicit RLCPayload(unsigned int num_bits) : num_bits(num_bits) {}
					
					unsigned int getBits() const override {
						return num_bits;
					}
				
				protected:
					unsigned int num_bits;
			};
			
			explicit RLCLayer(const MacId& own_id) : own_id(own_id) {}
			
			virtual ~RLCLayer() {
				for (L2Packet* packet : injections)
					delete packet;
			}
			
			void receiveFromUpper(L3Packet* data, MacId dest, PacketPriority priority) override {
			
			}
			
			void receiveFromLower(L2Packet* packet) override {
				coutd << "RLC received packet... ";
			}
			
			void receiveInjectionFromLower(L2Packet* packet, PacketPriority priority) override {
				coutd << "RLC received injection... ";
				injections.push_back(packet);
			}
			
			L2Packet* requestSegment(unsigned int num_bits, const MacId& mac_id) override {
				coutd << "RLC::requestSegment... ";
				if (injections.empty()) {
					coutd << "returning new unicast... ";
					auto* segment = new L2Packet();
					auto* base_header = new L2HeaderBase(own_id, 0, 0, 0);
					auto* unicast_header = new L2HeaderUnicast(mac_id, true, SequenceNumber(0), SequenceNumber(0), 0);
					segment->addPayload(base_header, new RLCPayload(0));
					segment->addPayload(unicast_header,
					                    new RLCPayload(num_bits - base_header->getBits() - unicast_header->getBits()));
					return segment;
				} else {
					coutd << "returning injection... ";
					L2Packet* injection = injections.at(injections.size() - 1);
					injections.pop_back();
					return injection;
				}
			}
			
			std::vector<L2Packet*> injections;
		protected:
			MacId own_id;
	};
	
	class NetworkLayer : public INet {
		public:
			unsigned int getNumHopsToGroundStation() const override {
				return 3;
			}
	};
	
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_MOCKLAYERS_HPP
