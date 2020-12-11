//
// Created by Sebastian Lindner on 10.12.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_MOVINGAVERAGE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_MOVINGAVERAGE_HPP


#include <vector>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class MovingAverage {
			
			friend class LinkManagerTests;
		
		public:
			explicit MovingAverage(unsigned int num_values);
			
			void put(unsigned long value);
			
			double get() const;
		
		protected:
			std::vector<unsigned long long> values;
			size_t index;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_MOVINGAVERAGE_HPP
