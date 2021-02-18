//
// Created by seba on 2/18/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_NEWLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_NEWLINKMANAGER_HPP

#include <MacId.hpp>
#include <random>
#include "ReservationManager.hpp"
#include "MCSOTDMA_Mac.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class NewLinkManager {
		friend class NewLinkManagerTests;

	public:
		enum Status {
			/** Communication is taking place. */
			link_established,
			/** Link has not been established yet. */
			link_not_established,
			/** Link establishment request has been prepared and we're waiting for the reply. */
			awaiting_reply,
			/** Link establishment reply has been prepared and we're waiting for the first message. */
			awaiting_data_tx,
			/** Link renewal has been completed. After expiry, the new reservations take action. */
			link_renewal_complete
		};

		/**
		 * @param link_id The link ID that should be managed.
		 * @param reservation_manager
		 * @param mac
		 */
		NewLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac);

		virtual ~NewLinkManager();

	protected:
		/** The ID of the managed link. */
		const MacId link_id;
		/** Points to the local ReservationManager, which gives access to ReservationTables. */
		ReservationManager *reservation_manager;
		/** Points to the MCSOTDMA MAC parent. */
		MCSOTDMA_Mac *mac;
		/** The current link status. */
		Status link_status;
		std::random_device* random_device;
		std::mt19937 generator;
	};

}

#endif //TUHH_INTAIRNET_MC_SOTDMA_NEWLINKMANAGER_HPP
