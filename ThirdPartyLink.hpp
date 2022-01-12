//
// Created by Sebastian Lindner on 01/12/22.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_THIRDPARTYLINK_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_THIRDPARTYLINK_HPP

#include <MacId.hpp>

namespace TUHH_INTAIRNET_MCSOTDMA {

/**
 * Handles locking and freeing resources as link requests and replies are received from users, whose links are not concerned with this user.
 * For example, if a link request indicates that a set of resources could soon be used, then these are locked.
 * When the corresponding link reply comes in, candidate resources are unlocked and the selected one scheduled.
 * If no reply comes in, or unexpected link requests, then these are processed adequately, as well.
 */
class ThirdPartyLink {
	public:

	protected:
		/** ID of the link initiator. */
		MacId id_link_initiator;
		/** ID of the link recipient. */
		MacId id_link_recipient;
};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_THIRDPARTYLINK_HPP