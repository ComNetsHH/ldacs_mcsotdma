//
// Created by Sebastian Lindner on 01/12/22.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_THIRDPARTYLINK_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_THIRDPARTYLINK_HPP

#include <MacId.hpp>
#include <L2Packet.hpp>
#include "LinkManager.hpp"
#include "ReservationMap.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

class MCSOTDMA_Mac;

/**
 * Handles locking and freeing resources as link requests and replies are received from users, whose links are not concerned with this user.
 * For example, if a link request indicates that a set of resources could soon be used, then these are locked.
 * When the corresponding link reply comes in, candidate resources are unlocked and the selected one scheduled.
 * If no reply comes in, or unexpected link requests, then these are processed adequately, as well.
 */
class ThirdPartyLink {

	friend class ThirdPartyLinkTests;

	public:
		ThirdPartyLink(const MacId &id_link_initiator, const MacId &id_link_recipient, MCSOTDMA_Mac *mac);
		
		void onSlotStart(size_t num_slots);
		void onSlotEnd();

		void processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload);
		void processLinkReplyMessage(const L2HeaderLinkReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id);
		const MacId& getIdLinkInitiator() const;
		const MacId& getIdLinkRecipient() const;

	protected:
		void reset();

	protected:
		static const int UNSET = -2;
		/** ID of the link initiator. */
		MacId id_link_initiator;
		/** ID of the link recipient. */
		MacId id_link_recipient;		
		/** Keeps track of locked resources. */
		ReservationMap locked_resources_for_initiator, locked_resources_for_recipient;
		ReservationMap scheduled_resources;
		/** Counter until an expected link reply. Once set, this is decremented each slot. */
		int num_slots_until_expected_link_reply = UNSET;
		/** Constant number of slots until an expected link reply. It is set together with num_slots_until_expected_link_reply, but this variable is not decremented. */
		int reply_offset = UNSET;		
		/** Set when a link reply is processed, this counter is decremented each slot and indicates when a link will terminate. */
		int link_expiry_offset = UNSET;
		MCSOTDMA_Mac *mac;
};

inline std::ostream& operator<<(std::ostream& stream, const ThirdPartyLink& link) {
	return stream << "ThirdPartyLink(" << link.getIdLinkInitiator() << ", " << link.getIdLinkRecipient() << ")";
}
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_THIRDPARTYLINK_HPP