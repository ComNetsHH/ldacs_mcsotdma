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
		enum Status {
			/** not currently in use and has not made any reservations */
			uninitialied,
			/** a request has been processed and resources may have been locked */
			received_request_awaiting_reply,
			/** a reply has been processed and resources may have been marked */
			received_reply_link_established
		};

		ThirdPartyLink(const MacId &id_link_initiator, const MacId &id_link_recipient, MCSOTDMA_Mac *mac);
		
		void onSlotStart(size_t num_slots);
		void onSlotEnd();

		void processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload);
		void processLinkReplyMessage(const L2HeaderLinkReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id);

		/** When another ThirdPartyLink is reset, some resources may have been unlocked or unscheduled. This function is then triggered, which might lock/schedule something on this link. */
		void onAnotherThirdLinkReset();
		const MacId& getIdLinkInitiator() const;
		const MacId& getIdLinkRecipient() const;
		bool operator==(const ThirdPartyLink &other);
		bool operator!=(const ThirdPartyLink &other);
		ThirdPartyLink::Status getStatus() const;		

	protected:
		void reset();

		/**
		 * Attempts to lock resources along all proposed links.
		 * @param locks_initiator Reference to the map that saves resources locked for the link initiator.
		 * @param locks_recipient Reference to the map that saves resources locked for the link recipient.
		 * @param proposed_resources 
		 * @param normalization_offset Add this value to each start slot in proposed_resources
		 * @param burst_length 
		 * @param burst_length_tx 
		 * @param burst_length_rx 
		 * @param burst_offset 
		 * @param timeout 
		 */
		void lockIfPossible(ReservationMap& locks_initiator, ReservationMap& locks_recipient, const std::map<const FrequencyChannel*, std::vector<unsigned int>> &proposed_resources, const int &normalization_offset, const int &burst_length, const int &burst_length_tx, const int &burst_length_rx, const int &burst_offset, const int &timeout);

	protected:
		class LinkDescription {
			public:
				LinkDescription() : proposed_resources(), burst_length(0), burst_length_tx(0), burst_length_rx(0), burst_offset(0), timeout(0) {}

				LinkDescription(const std::map<const FrequencyChannel*, std::vector<unsigned int>> &proposed_resources, const int &burst_length, const int &burst_length_tx, const int &burst_length_rx, const int &burst_offset, const int &timeout)
					: proposed_resources(proposed_resources), burst_length(burst_length), burst_length_tx(burst_length_tx), burst_length_rx(burst_length_rx), burst_offset(burst_offset), timeout(timeout) {}

				explicit LinkDescription(const LinkDescription &other) : LinkDescription(other.proposed_resources, other.burst_length, other.burst_length_tx, other.burst_length_rx, other.burst_offset, other.timeout) {}				

				/** Set after request reception. */
				std::map<const FrequencyChannel*, std::vector<unsigned int>> proposed_resources;				
				int burst_length, burst_length_tx, burst_length_rx, burst_offset, timeout;
				/** Set after reply reception. */
				const FrequencyChannel *selected_channel = nullptr;
				/** Set after reply reception. */
				int first_burst_in;
		};

		ThirdPartyLink::Status status = uninitialied;
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
		/** Set when a request or reply has been received, and then incremented each slot. */
		int normalization_offset = UNSET;
		MCSOTDMA_Mac *mac;
		LinkDescription link_description;
};

inline std::ostream& operator<<(std::ostream& stream, const ThirdPartyLink& link) {
	return stream << "ThirdPartyLink(" << link.getIdLinkInitiator() << ", " << link.getIdLinkRecipient() << ")";
}

inline std::ostream& operator<<(std::ostream& stream, const ThirdPartyLink::Status& status) {
	std::string str;
	switch (status) {
		case ThirdPartyLink::uninitialied: {
			str = "uninitialied";
			break;
		}
		case ThirdPartyLink::received_request_awaiting_reply: {
			str = "received_request_awaiting_reply";
			break;
		}
		case ThirdPartyLink::received_reply_link_established: {
			str = "received_reply_link_established";
			break;
		}
		default: {
			throw std::runtime_error("operator<< for unrecognized status: " + std::to_string(status));
			break;
		}
	}
	return stream << str;
}


}

#endif //TUHH_INTAIRNET_MC_SOTDMA_THIRDPARTYLINK_HPP