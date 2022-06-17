#ifndef TUHH_INTAIRNET_MC_SOTDMA_NEIGHBOROBSERVER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_NEIGHBOROBSERVER_HPP


#include <MacId.hpp>
#include <map>
#include <vector>

namespace TUHH_INTAIRNET_MCSOTDMA {
	/**
	 * Keeps track of recently observed, active neighbors.
	 */
	class NeighborObserver {
	public:
		NeighborObserver(unsigned int max_time_slots_until_neighbor_not_active_anymore);

		void reportActivity(const MacId& id);
		void reportBroadcastSlotAdvertisement(const MacId& id, unsigned int advertised_slot_offset);
		unsigned int getNextExpectedBroadcastSlotOffset(const MacId &id) const;
		void onSlotEnd();
		size_t getNumActiveNeighbors() const;
		bool isActive(const MacId& id) const;
		std::vector<MacId> getActiveNeighbors() const;
 
	protected:
		/** Pairs of <ID, last-seen-this-many-slots-ago> */
		std::map<MacId, unsigned int> active_neighbors;
		/** Pairs of <ID, next-broadcast> */
		std::map<MacId, unsigned int> advertised_broadcast_slots;

		unsigned int max_last_seen_val;
	};
}
#endif