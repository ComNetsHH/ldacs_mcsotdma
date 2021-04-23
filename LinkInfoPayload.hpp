//
// Created by seba on 4/21/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKINFOPAYLOAD_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKINFOPAYLOAD_HPP

#include <L2Packet.hpp>
#include <utility>
#include "LinkInfo.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * When a new point-to-point link is established, a LinkInfo message should be broadcast, informing neighbors of the new resource utilization.
	 */
	class LinkInfoPayload : public L2Packet::Payload {
	public:
		class Callback {
		public:
			virtual LinkInfo getLinkInfo() = 0;
		};

		explicit LinkInfoPayload(Callback *callback) : callback(callback) {}

		Payload* copy() const override {
			auto *copy = new LinkInfoPayload(this->callback);
			copy->link_info = LinkInfo(link_info);
			return copy;
		}

		unsigned int getBits() const override {
			return link_info.getBits();
		}

		const LinkInfo& getLinkInfo() const {
			return link_info;
		}

		void populate() {
			if (callback == nullptr)
				throw std::runtime_error("LinkInfoPayload::populate for unset callback.");
			coutd << "populating link info payload: ";
			link_info = callback->getLinkInfo();
			coutd << " -> ";
		}

	protected:
		LinkInfo link_info;
		Callback *callback = nullptr;
	};

}

#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKINFOPAYLOAD_HPP
