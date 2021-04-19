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
			coutd << "PHY::receiveFromUpper(" << data->getBits() << "bits, " << center_frequency << "kHz)";
			if (connected_phy == nullptr) {
				coutd << " -> buffered." << std::endl;
				outgoing_packets.push_back(data);
				outgoing_packet_freqs.push_back(center_frequency);
			} else {
				coutd << " -> sent." << std::endl;
				outgoing_packets.push_back(data->copy());
				outgoing_packet_freqs.push_back(center_frequency);
				connected_phy->onReception(data, center_frequency);
			}
		}

		unsigned long getCurrentDatarate() const override {
			return 1600; // 200B/slot
		}

		~PHYLayer() override {
			for (L2Packet* packet : outgoing_packets)
				delete packet;
		}

		std::vector<L2Packet*> outgoing_packets;
		std::vector<unsigned int> outgoing_packet_freqs;
		PHYLayer* connected_phy = nullptr;
	};

	class MACLayer : public MCSOTDMA_Mac {

		friend class LinkManagerTests;
		friend class BCLinkManagerTests;
		friend class SystemTests;
		friend class NewSystemTests;
		friend class TestEnvironment;

	public:
		explicit MACLayer(const MacId& id, uint32_t planning_horizon) : MCSOTDMA_Mac(id, planning_horizon) {

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

			RLCPayload(const RLCPayload& other) : num_bits(other.num_bits) {}

			unsigned int getBits() const override {
				return num_bits;
			}

			Payload* copy() const override {
				return new RLCPayload(*this);
			}

		protected:
			unsigned int num_bits;
		};

		explicit RLCLayer(const MacId& own_id) : own_id(own_id) {}

		virtual ~RLCLayer() {
			for (auto it = control_message_injections.begin(); it != control_message_injections.end(); it++)
				for (L2Packet* packet : it->second)
					delete packet;
			for (L2Packet* packet : receptions)
				delete packet;
		}

		void receiveFromUpper(L3Packet* data, MacId dest, PacketPriority priority) override {

		}

		void receiveFromLower(L2Packet* packet) override {
			coutd << "RLC received packet... ";
			receptions.push_back(packet);
		}

		void receiveInjectionFromLower(L2Packet* packet, PacketPriority priority) override {
			coutd << "RLC received injection for '" << packet->getDestination() << "'... ";
			auto it = control_message_injections.find(packet->getDestination());
			if (it == control_message_injections.end()) {
				control_message_injections[packet->getDestination()] = std::vector<L2Packet*>();
				control_message_injections[packet->getDestination()].push_back(packet);
			} else
				it->second.push_back(packet);
			lower_layer->notifyOutgoing(packet->getBits(), packet->getDestination());
		}

		L2Packet* requestSegment(unsigned int num_bits, const MacId& mac_id) override {
			coutd << "RLC::requestSegment -> ";
			L2Packet* segment;
			if (control_message_injections.find(mac_id) == control_message_injections.end() || control_message_injections.at(mac_id).empty()) {
				// Broadcast...
				if (mac_id == SYMBOLIC_LINK_ID_BROADCAST) {
					coutd << "returning new broadcast -> ";
					segment = new L2Packet();
					auto* base_header = new L2HeaderBase();
					auto* broadcast_header = new L2HeaderBroadcast();
					segment->addMessage(base_header, nullptr);
					segment->addMessage(broadcast_header, new RLCPayload(num_bits));
				} else {
					coutd << "returning new unicast -> ";
					segment = new L2Packet();
					auto* base_header = new L2HeaderBase(own_id, 0, 0, 0, 0);
					auto* unicast_header = new L2HeaderUnicast(mac_id, true, SequenceNumber(0), SequenceNumber(0), 0);
					segment->addMessage(base_header, new RLCPayload(0));
					segment->addMessage(unicast_header, new RLCPayload(num_bits - base_header->getBits() - unicast_header->getBits()));
				}
			} else {
				coutd << "returning injection -> ";
				segment = control_message_injections.at(mac_id).at(control_message_injections.at(mac_id).size() - 1);
				control_message_injections.at(mac_id).pop_back();
			}
			return segment;
		}

		bool isThereMoreData(const MacId& mac_id) const override {
			if (mac_id == SYMBOLIC_LINK_ID_BROADCAST)
				return should_there_be_more_broadcast_data;
			else
				return should_there_be_more_p2p_data;
		}

		std::map<MacId, std::vector<L2Packet*>> control_message_injections;
		std::vector<L2Packet*> receptions;
		bool should_there_be_more_p2p_data = true, should_there_be_more_broadcast_data = false;
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

		void receiveFromLower(L3Packet* packet) override {

		}

		std::map<MacId, unsigned int> num_hops_to_GS_map;
	};

	class TestEnvironment {
	public:
		MacId id, partner_id;
		uint32_t planning_horizon = 512;
		uint64_t center_frequency1 = 962, center_frequency2 = 963, center_frequency3 = 964, bc_frequency = 965, bandwidth = 500;
		NetworkLayer* net_layer;
		RLCLayer* rlc_layer;
		ARQLayer* arq_layer;
		MACLayer* mac_layer;
		PHYLayer* phy_layer;

		TestEnvironment(const MacId& own_id, const MacId& communication_partner_id, bool use_new_link_manager) : id(own_id), partner_id(communication_partner_id) {
			phy_layer = new PHYLayer(planning_horizon);
			mac_layer = new MACLayer(own_id, planning_horizon);
			mac_layer->reservation_manager->setTransmitterReservationTable(phy_layer->getTransmitterReservationTable());
			for (ReservationTable*& table : phy_layer->getReceiverReservationTables())
				mac_layer->reservation_manager->addReceiverReservationTable(table);

			mac_layer->reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
			mac_layer->reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
			mac_layer->reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
			mac_layer->reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);

			arq_layer = new ARQLayer();
			arq_layer->should_forward = true;
			mac_layer->setUpperLayer(arq_layer);
			arq_layer->setLowerLayer(mac_layer);
			net_layer = new NetworkLayer();
			rlc_layer = new RLCLayer(own_id);
			net_layer->setLowerLayer(rlc_layer);
			rlc_layer->setUpperLayer(net_layer);
			rlc_layer->setLowerLayer(arq_layer);
			arq_layer->setUpperLayer(rlc_layer);
			phy_layer->setUpperLayer(mac_layer);
			mac_layer->setLowerLayer(phy_layer);
		}

		TestEnvironment(const MacId& own_id, const MacId& communication_partner_id) : TestEnvironment(own_id, communication_partner_id, false) {}

		virtual ~TestEnvironment() {
			delete mac_layer;
			delete arq_layer;
			delete rlc_layer;
			delete phy_layer;
			delete net_layer;
		}
	};

}

#endif //TUHH_INTAIRNET_MC_SOTDMA_MOCKLAYERS_HPP
