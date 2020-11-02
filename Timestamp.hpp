//
// Created by Sebastian Lindner on 02.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_TIMESTAMP_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_TIMESTAMP_HPP

#include <cstdint>

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * Implements a notion of time just as OMNeT++ does. Could be derived from for a different format.
	 */
	class Timestamp {
		public:
			Timestamp() : t(0) {}
			Timestamp(int64_t t) : t(t) {}
			
			int64_t t;
			
			bool operator==(const Timestamp& x) const  {return t==x.t;}
			bool operator!=(const Timestamp& x) const  {return t!=x.t;}
			bool operator< (const Timestamp& x) const  {return t<x.t;}
			bool operator> (const Timestamp& x) const  {return t>x.t;}
			bool operator<=(const Timestamp& x) const  {return t<=x.t;}
			bool operator>=(const Timestamp& x) const  {return t>=x.t;}
			Timestamp& operator=(const Timestamp& x) {t=x.t; return *this;}
			Timestamp& operator=(const int64_t x) {t=x; return *this;}
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_TIMESTAMP_HPP
