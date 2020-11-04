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
			
			virtual bool operator==(const IcaoId& other) const {
				return this->id == other.id;
			}
			
			bool operator!=(const IcaoId& other) const {
				return !(*this == other);
			}
			
			unsigned int getBits() const {
				return 27; // ICAO ID is 27 bits.
			}
		
		protected:
			int id;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_USERID_HPP
