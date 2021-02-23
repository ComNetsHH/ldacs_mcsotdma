//
// Created by seba on 1/31/21.
//

#include "BCLinkManagementEntity.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "OldLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

TUHH_INTAIRNET_MCSOTDMA::BCLinkManagementEntity::BCLinkManagementEntity(OldLinkManager* owner) : LinkManagementEntity(owner) {}

void BCLinkManagementEntity::processLinkReply(const L2HeaderLinkEstablishmentReply*& header, const LinkManagementEntity::ProposalPayload*& payload) {
	throw std::runtime_error("BCLinkManagementEntity cannot process link replies.");
}

void BCLinkManagementEntity::processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header, const LinkManagementEntity::ProposalPayload*& payload, const MacId& origin) {
	// Forward request to the corresponding OldLinkManager.
	coutd << "forwarding link request to OldLinkManager(" << origin << ") -> ";
	auto* request = new L2Packet();
	request->addMessage(new L2HeaderBase(origin, 0, 0, 0, 0), nullptr);
	request->addMessage(new L2HeaderLinkEstablishmentRequest(*header), payload->copy());
	owner->mac->getLinkManager(origin)->onPacketReception(request);
}