//
// Created by Sebastian Lindner on 10.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_CPRPOSITION_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_CPRPOSITION_HPP

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/** The Compact Position Report-encoded position of latitude, longitude, altitude as ADS-B uses it. */
	class CPRPosition {
		public:
			CPRPosition(double latitude, double longitude, double altitude) : latitude(latitude), longitude(longitude), altitude(altitude) {
				// No actual computation is performed.
			}
			
			CPRPosition(const CPRPosition& other) = default;
			
			double latitude, longitude, altitude;
			
			/** Number of bits required to encode this position. */
			unsigned int getBits() const {
				return 12 /* latitude */ + 14 /* longitutde */ + 12 /* altitude */;
			}
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_CPRPOSITION_HPP
