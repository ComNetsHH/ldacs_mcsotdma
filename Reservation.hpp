//
// Created by Sebastian Lindner on 04.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP

#include "UserId.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

/**
 * A Reservation can be associated to time slots and is used to denote owner(s) of a communication link.
 * If the current user owns this reservation, it may further specify whether the slot should be used to receive or transmit data.
 */
	class Reservation {
		public:
			/** What the slot that is associated to this reservation should be used for. */
			enum Action {
					RX, TX, UNSET
			};
			
			Reservation(UserId owner, Action action);
			
			explicit Reservation(UserId owner);
			
			/**
			 * @return The ID of the holder of this reserved slot.
			 */
			const UserId& getOwner() const;
			
			/**
			 * @return If the owner of the reservation is the current user, then the action shall define whether the
			 * slot is used for reception (RX) or transmission (TX). For other users it'll be unset (UNSET).
			 */
			const Action& getAction() const;
			
			void setAction(Action action);
		
		protected:
			UserId owner;
			Action action;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP
