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

#ifndef TUHH_INTAIRNET_MC_SOTDMA_NEIGHBOROBSERVER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_NEIGHBOROBSERVER_HPP


#include <MacId.hpp>
#include <map>
#include <vector>
#include "LinkProposal.hpp"
#include "MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	/**
	 * Keeps track of recently observed, active neighbors.
	 */
	class NeighborObserver {

		friend class SystemTests;

	public:
		NeighborObserver(unsigned int max_time_slots_until_neighbor_not_active_anymore);
		
		void reportActivity(const MacId& id);
		void reportBroadcastSlotAdvertisement(const MacId& id, unsigned int advertised_slot_offset);
		unsigned int getNextExpectedBroadcastSlotOffset(const MacId &id) const;
		void onSlotEnd();
		size_t getNumActiveNeighbors() const;
		bool isActive(const MacId& id) const;
		std::vector<MacId> getActiveNeighbors() const;
		void clearAdvertisedLinkProposals(const MacId &id);
		void addAdvertisedLinkProposal(const MacId &id, unsigned long current_slot, const LinkProposal &proposal);
		std::vector<LinkProposal> getAdvertisedLinkProposals(const MacId &id, const unsigned long current_slot) const;
		/** @return The average over the average last-seen of all neighbors. */
		double getAvgBeaconDelay() const;
		/** @return The average time in-between beacon receptions of the first neighbor whose beacon has been received. */
		double getAvgFirstNeighborBeaconDelay() const;

	protected:		
		/** Sets the respective value in active_neighbors, which is incremented each slot. 
		 * @return The number of time slots since this user was last seen.
		*/
		uint64_t updateLastSeenCounter(const MacId &id);
		/** Updates the respective average value of the number of time slots in-between beacon receptions. */
		void updateAvgLastSeen(const MacId &id, uint64_t num_time_slots_since_last_seen);
 
	protected:
		/** Pairs of <ID, last-seen-this-many-slots-ago> */
		std::map<MacId, unsigned int> active_neighbors;
		/** Pairs of <ID, next-broadcast> */
		std::map<MacId, unsigned int> advertised_broadcast_slots;
		std::map<MacId, std::vector<std::pair<unsigned long, LinkProposal>>> advertised_link_proposals;
		std::map<MacId, MovingAverage> avg_last_seen;
		/** Number of time slots to use for the moving averages in avg_last_seen.*/
		const uint64_t num_time_slots_to_average = 10;
		/** Average time in-between beacons of the first user whose beacon has been received. */
		MovingAverage first_neighbor_avg_last_seen;
		MacId first_neighbor_id = SYMBOLIC_ID_UNSET;		

		unsigned int max_last_seen_val;
	};
}
#endif