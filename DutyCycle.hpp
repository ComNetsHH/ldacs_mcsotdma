//
// Created by Sebastian Lindner on 26.05.22.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_DUTYCYCLE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_DUTYCYCLE_HPP

#include "MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class MCSOTDMA_Mac;

	/**	 
	 * Budget calculations with regard to the duty cycle.
	 */
	class DutyCycle {
		public:
			DutyCycle();

			/**			 
			 * @param period Number of time slots to consider when computing the duty cycle.
			 * @param max_duty_cycle Maximum duty cycle as a percentage.
			 */
			DutyCycle(unsigned int period, double max_duty_cycle, MCSOTDMA_Mac *mac);

			/**
			 * During each time slot, the number of transmissions should be reported so that the DutyCycle can keep an accurate measure.
			 * @param num_txs 
			 */
			void reportNumTransmissions(unsigned int num_txs);

			/**
			 * @return Whether enough values have been captured to provide an accurate measure.
			 */
			bool shouldEmitStatistic() const;

			/**			 
			 * @return Current duty cycle.
			 */
			double get() const;

			/**			 
			 * @param n Number of PP links that should be supported simultaneously.
			 */
			void setMinNumSupportedPPLinks(unsigned int n);

			/**			 
			 * @return Minimum number of time slots in-between two transmission bursts so that the duty cycle budget is maintained.
			 */
			unsigned int getPeriodicity(bool sh_channel_access) const;

		protected:
			/** Number of time slots to consider when computing the duty cycle. */
			unsigned int period;
			/** Maximum duty cycle as a percentage. */
			double max_duty_cycle;
			unsigned int min_num_supported_pp_links = 4;
			MovingAverage duty_cycle;			
			MCSOTDMA_Mac *mac;
	};
}


#endif