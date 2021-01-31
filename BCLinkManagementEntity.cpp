//
// Created by seba on 1/31/21.
//

#include "BCLinkManagementEntity.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

TUHH_INTAIRNET_MCSOTDMA::BCLinkManagementEntity::BCLinkManagementEntity(LinkManager* owner) : LinkManagementEntity(owner) {}

void BCLinkManagementEntity::processLinkReply(const L2HeaderLinkEstablishmentReply*& header, const LinkManagementEntity::ProposalPayload*& payload) {
	throw std::runtime_error("BCLinkManagementEntity cannot process link replies.");
}

void BCLinkManagementEntity::processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header, const LinkManagementEntity::ProposalPayload*& payload, const MacId& origin) {
	// Forward request to the corresponding LinkManager.
	coutd << "forwarding link request to LinkManager(" << origin << ") -> ";
	auto* request = new L2Packet();
	request->addPayload(new L2HeaderBase(origin, 0, 0, 0), nullptr);
	request->addPayload(new L2HeaderLinkEstablishmentRequest(*header), payload->copy());
	owner->mac->getLinkManager(origin)->receiveFromLower(request);
}