//
// Created by Sebastian Lindner on 11.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_L2LAYER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_L2LAYER_HPP

#include "QueueManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * The Data Link Layer class instantiates all required components and connects them.
	 */
	class L2Layer {
		public:
			L2Layer(uint32_t reservation_planning_horizon);
			virtual ~L2Layer();
		
		protected:
			QueueManager* queue_manager;
			ReservationManager* reservation_manager;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_L2LAYER_HPP
