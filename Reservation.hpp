// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
			/** Reservation for me, and I should *listen* for a beacon during this slot. */
			RX_BEACON,
			/** Reservation for me, and I should *start to transmit* during this slot. */
			TX,			
			/** Reservation for me, and i should *transmit* a beacon during this slot. */
			TX_BEACON,
			/** A locked reservation has been considered in a link request proposal and shouldn't be used until this negotiation has concluded. */
			LOCKED
		};

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

		void setTarget(const MacId& target);

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
		 * @return Whether this denotes any type of transmission.
		 */
		bool isAnyTx() const;

		/**
		 * @return Whether this denotes a reception resource.
		 */
		bool isRx() const;		

		/**
		 * @return Whether this denotes a resource for beacon reception.
		 */
		bool isBeaconRx() const;

		/**
		 * @return Whether this denotes any type of reception.
		 */
		bool isAnyRx() const;

		/**
		 * @return Whether this denotes a resource for beacon transmission.
		 */
		bool isBeaconTx() const;

		/**
		 * @return Whether this denotes a resource for beacon reception or transmission.
		 */
		bool isBeacon() const;

		/**
		 * @return Whether this resource is locked as it was used for making a proposal and shouldn't be considered for further reservations until the negotiation has concluded.
		 */
		bool isLocked() const;

		std::string toString() const;

	protected:
		/** Target MAC ID. */
		MacId target;
		Action action;		
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
			case Reservation::RX_BEACON: {
				str = "RX_BEACON";
				break;
			}
			case Reservation::TX: {
				str = "TX";
				break;
			}			
			case Reservation::TX_BEACON: {
				str = "TX_BEACON";
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
