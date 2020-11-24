//
// Created by Sebastian Lindner on 04.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP

#include "MacId.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class LinkManager;

	/**
	 * A Reservation can be associated to time slots and is used to denote owner(s) of a communication link.
	 * If the current user owns this reservation, it may further specify whether the slot should be used to receive or transmit data.
	 */
	class Reservation {
		public:
			/** What the slot that is associated to this reservation should be used for. */
			enum Action {
				/** No reservation. */
				IDLE,
				/** Reservation for some other user. */
				BUSY,
				/** Reservation for me, and I should *listen* during this slot. */
				RX,
				/** Reservation for me, and I should *transmit* during this slot. */
				TX
			};
			
			Reservation(MacId owner, Action action);
			explicit Reservation(MacId owner);
			Reservation();
			virtual ~Reservation();
			
			/**
			 * @return The MAC ID of whoever holds this reservation.
			 */
			const MacId& getOwner() const;
			
			/**
			 * @return If the owner of the reservation is the current user, then the action shall define whether the
			 * slot is used for reception (RX) or transmission (TX). For other users it'll be unset (UNSET).
			 */
			const Action& getAction() const;
			
			void setAction(Action action);
			
			/**
			 * Sets a pointer to the LinkManager that created this reservation.
			 * @param creator
			 */
			void setCreator(LinkManager* creator);
			/**
			 * @return The LinkManager that created this reservation.
			 */
			LinkManager* getCreator();
		
		protected:
			/** Reservations are made by MAC nodes, so hold the ID of this reservation's holder. */
			MacId owner;
			Action action;
			LinkManager* creator = nullptr;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP
