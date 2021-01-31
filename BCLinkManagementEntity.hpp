//
// Created by seba on 1/31/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGEMENTENTITY_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGEMENTENTITY_HPP

#include "LinkManagementEntity.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class BCLinkManagementEntity : public LinkManagementEntity {
		friend class LinkManagementEntityTests;

		friend class LinkManagerTests;

		friend class BCLinkManagerTests;

		friend class SystemTests;

	public:
		explicit BCLinkManagementEntity(LinkManager* owner);

		void processLinkReply(const L2HeaderLinkEstablishmentReply*& header, const ProposalPayload*& payload) override;

		void processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header, const ProposalPayload*& payload, const MacId& origin) override;

	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGEMENTENTITY_HPP
