//
// Created by Sebastian Lindner on 04.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP

#include <stdexcept>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class IcaoId {
		public:
			explicit IcaoId(int id) : id(id) {}
			
			IcaoId(const IcaoId& other)	= default;
			
			bool operator!=(const IcaoId& other) const {
				return !(*this == other);
			}
			
			unsigned int getBits() const {
				return 27; // ICAO ID is 27 bits.
			}
			
			const int& getId() const {
				return this->id;
			}
			
			bool operator==(const IcaoId& other) const {
				return this->id == other.id;
			}
			bool operator<(const IcaoId& other) const {
				return this->id < other.id;
			}
			bool operator<=(const IcaoId& other) const {
				return this->id <= other.id;
			}
			bool operator>(const IcaoId& other) const {
				return this->id > other.id;
			}
			bool operator>=(const IcaoId& other) const {
				return this->id >= other.id;
			}
		
		protected:
			int id;
	};
	
	/** Symbolic global ID that represents an unset ICAO ID. */
	const IcaoId ICAO_ID_UNSET = IcaoId(-1);
	/** Symbolic global ID that represents a broadcast. */
	const IcaoId ICAO_ID_BROADCAST = IcaoId(-2);
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP
