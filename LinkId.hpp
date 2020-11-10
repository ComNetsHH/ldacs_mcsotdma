//
// Created by Sebastian Lindner on 04.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP

#include <stdexcept>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class LinkId {
		public:
			explicit LinkId(int id) : id(id) {}
			
			LinkId(const LinkId& other)	= default;
			
			bool operator!=(const LinkId& other) const {
				return !(*this == other);
			}
			
			unsigned int getBits() const {
				return 27; // ICAO ID is 27 bits.
			}
			
			const int& getId() const {
				return this->id;
			}
			
			bool operator==(const LinkId& other) const {
				return this->id == other.id;
			}
			bool operator<(const LinkId& other) const {
				return this->id < other.id;
			}
			bool operator<=(const LinkId& other) const {
				return this->id <= other.id;
			}
			bool operator>(const LinkId& other) const {
				return this->id > other.id;
			}
			bool operator>=(const LinkId& other) const {
				return this->id >= other.id;
			}
		
		protected:
			int id;
	};
	
	/** Symbolic global ID that represents an unset ICAO ID. */
	const LinkId LINK_ID_UNSET = LinkId(-1);
	/** Symbolic global ID that represents a broadcast. */
	const LinkId LINK_ID_BROADCAST = LinkId(-2);
	/** Symbolic global ID that represents a beacon (which is also a broadcast). */
	const LinkId LINK_ID_BEACON = LinkId(-3);
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP
