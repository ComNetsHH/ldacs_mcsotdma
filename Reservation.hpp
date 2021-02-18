//
// Created by Sebastian Lindner on 04.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP

#include "MacId.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class OldLinkManager;

	/**
	 * A Reservation can be associated to time slots and is used to denote target(s) of a communication link.
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
			/** Reservation for me, and I should *continue listening* during this slot. */
			RX_CONT,
			/** Reservation for me, and I should *start to transmit* during this slot. */
			TX,
			/** Reservation for me, and I should *continue transmitting* during this slot. */
			TX_CONT,
			/** A locked reservation has been considered in a link request proposal and shouldn't be used until this negotiation has concluded. */
			LOCKED
		};

		/**
		 * @param target
		 * @param action
		 * @param num_remaining_slots For a continuous burst, the number of slots *after* this one that should also be used for this transmission burst, may be incorporated into a Reservation.
		 */
		Reservation(const MacId& target, Action action, unsigned int num_remaining_slots);

		Reservation(const MacId& target, Action action);

		explicit Reservation(const MacId& target);

		Reservation();

		Reservation(const Reservation& other);

		virtual ~Reservation();

		/**
		 * @return The MAC ID of whoever holds this reservation.
		 */
		const MacId& getTarget() const;

		/**
		 * @return The current action associated with this resource.
		 */
		const Action& getAction() const;

		void setAction(Action action);

		/**
		 * @return Whether locking succeeded.
		 */
		bool lock();

		/**
		 * @return Number of remaining slots this transmission burst continues for.
		 */
		unsigned int getNumRemainingSlots() const;

		/**
		 * @param num_slots The number of slots this transmission burst continues for.
		 */
		void setNumRemainingSlots(const unsigned int& num_slots);

		bool operator==(const Reservation& other) const;

		bool operator!=(const Reservation& other) const;

		/**
		 * @return Whether this reservation can be considered as usable for making a new reservation.
		 */
		bool isIdle() const;

		/**
		 * @return Whether this denotes a resource that is reserved by another user.
		 */
		bool isBusy() const;

		/**
		 * @return Whether this denotes a reserved transmission resource.
		 */
		bool isTx() const;

		/**
		 * @return Whether this denotes a continued transmission resource.
		 */
		bool isTxCont() const;

		/**
		 * @return Whether this denotes a reception resource.
		 */
		bool isRx() const;

		/**
		 * @return Whether this denotes a continued reception resource.
		 */
		bool isRxCont() const;

		/**
		 * @return Whether this resource is locked as it was used for making a proposal and shouldn't be considered for further reservations until the negotiation has concluded.
		 */
		bool isLocked() const;

	protected:
		/** Target MAC ID. */
		MacId target;
		Action action;
		/** In case of a transmission, this keeps the number of remaining slots for this transmission burst. */
		unsigned int num_remaining_slots = 0;
	};

	inline std::ostream& operator<<(std::ostream& stream, const Reservation::Action& action) {
		std::string str;
		switch (action) {
			case Reservation::IDLE: {
				str = "IDLE";
				break;
			}
			case Reservation::BUSY: {
				str = "BUSY";
				break;
			}
			case Reservation::RX: {
				str = "RX";
				break;
			}
			case Reservation::TX: {
				str = "TX";
				break;
			}
			case Reservation::TX_CONT: {
				str = "TX_CONT";
				break;
			}
			case Reservation::RX_CONT: {
				str = "RX_CONT";
				break;
			}
			case Reservation::LOCKED: {
				str = "LOCKED";
				break;
			}
		}
		return stream << str;
	}

	inline std::ostream& operator<<(std::ostream& stream, const Reservation& reservation) {
		return stream << reservation.getAction() << std::string("@") << reservation.getTarget();
	}


}

#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATION_HPP
