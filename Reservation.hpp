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
				/** Reservation for me, and I should *start to transmit* during this slot. */
				TX,
				/** Reservation for me, and I should *continue transmitting* during this slot. */
				TX_CONT
			};
			
			/**
			 * @param owner
			 * @param action
			 * @param num_remaining_tx_slots For a continuous transmission burst, the number of slots *after* this one that should also be used
			 * for this transmission burst, may be incorporated into a Reservation.
			 */
			Reservation(const MacId& owner, Action action, unsigned int num_remaining_tx_slots);
			Reservation(const MacId& owner, Action action);
			explicit Reservation(const MacId& owner);
			Reservation();
			Reservation(const Reservation& other);
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
			 * @return Number of remaining slots this transmission burst continues for.
			 */
			unsigned int getNumRemainingTxSlots() const;
			
			/**
			 * @param num_slots The number of slots this transmission burst continues for.
			 */
			void setNumRemainingTxSlots(const unsigned int& num_slots);
			
			bool operator==(const Reservation& other) const;
			bool operator!=(const Reservation& other) const;
		
		protected:
			/** Reservations are made by MAC nodes, so hold the ID of this reservation's holder. */
			MacId owner;
			Action action;
			/** In case of a transmission, this keeps the number of remaining slots for this transmission burst. */
			unsigned int num_remaining_tx_slots = 0;
	};
	
	inline std::ostream & operator<<(std::ostream & stream, const Reservation& reservation) {
		std::string action;
		switch (reservation.getAction()) {
			case Reservation::IDLE:
				action = "IDLE";
				break;
			case Reservation::BUSY:
				action = "BUSY";
				break;
			case Reservation::RX:
				action = "RX";
				break;
			case Reservation::TX:
				action = "TX";
				break;
			case Reservation::TX_CONT:
				action = "TX_CONT";
				break;
		}
		action += "@";
		return stream << action << reservation.getOwner();
	}
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP
