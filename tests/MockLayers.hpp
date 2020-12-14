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
#include "../MCSOTDMA_Phy.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class PHYLayer : public MCSOTDMA_Phy {
		public:
			explicit PHYLayer(uint32_t planning_horizon) : MCSOTDMA_Phy(planning_horizon) {}
			
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
			
			friend class LinkManagerTests;
			friend class BCLinkManagerTests;
			friend class SystemTests;
			
		public:
			explicit MACLayer(const MacId& id, uint32_t planning_horizon) : MCSOTDMA_Mac(id, planning_horizon) {}
		
		protected:
			void onReceptionSlot(const FrequencyChannel* channel) override {
				// do nothing.
			}
	};
	
	class ARQLayer : public IArq {
		public:
			void notifyOutgoing(unsigned int num_bits, const MacId& mac_id) override {
				coutd << "ARQ::notifyOutgoing(bits=" << num_bits << ", id=" << mac_id << ")" << std::endl;
				if (should_forward)
					lower_layer->notifyOutgoing(num_bits, mac_id);
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
			
			bool should_forward = false;
			
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
				coutd << "RLC received injection for '" << packet->getDestination() << "'... ";
				injections.push_back(packet);
				lower_layer->notifyOutgoing(packet->getBits(), packet->getDestination());
			}
			
			L2Packet* requestSegment(unsigned int num_bits, const MacId& mac_id) override {
				coutd << "RLC::requestSegment -> ";
				L2Packet* segment;
				if (injections.empty()) {
					if (mac_id == SYMBOLIC_LINK_ID_BROADCAST) {
						coutd << "returning new broadcast -> ";
						segment = new L2Packet();
						auto* base_header = new L2HeaderBase();
						auto* broadcast_header = new L2HeaderBroadcast();
						segment->addPayload(base_header, nullptr);
						segment->addPayload(broadcast_header, new RLCPayload(num_bits));
					} else {
						coutd << "returning new unicast -> ";
						segment = new L2Packet();
						auto* base_header = new L2HeaderBase(own_id, 0, 0, 0);
						auto* unicast_header = new L2HeaderUnicast(mac_id, true, SequenceNumber(0), SequenceNumber(0), 0);
						segment->addPayload(base_header, new RLCPayload(0));
						segment->addPayload(unicast_header, new RLCPayload(num_bits - base_header->getBits() - unicast_header->getBits()));
					}
				} else {
					coutd << "returning injection -> ";
					segment = injections.at(injections.size() - 1);
					injections.pop_back();
				}
				return segment;
			}
			
			bool isThereMoreData(const MacId& mac_id) const override {
				return should_there_be_more_data;
			}
			
			std::vector<L2Packet*> injections;
			bool should_there_be_more_data = true;
		protected:
			MacId own_id;
	};
	
	class NetworkLayer : public INet {
		public:
			unsigned int getNumHopsToGroundStation() const override {
				return 3;
			}
			
			void reportNumHopsToGS(const MacId& id, unsigned int num_hops) override {
				num_hops_to_GS_map[id] = num_hops;
			}
			
			std::map<MacId, unsigned int> num_hops_to_GS_map;
	};
	
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_MOCKLAYERS_HPP
