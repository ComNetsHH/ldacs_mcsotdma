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

#ifndef TUHH_INTAIRNET_MC_SOTDMA_DUTYCYCLE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_DUTYCYCLE_HPP

#include "DutyCycleBudgetStrategy.hpp"
#include "MovingAverage.hpp"
#include <string>
#include <stdexcept>

namespace TUHH_INTAIRNET_MCSOTDMA {		

	class no_duty_cycle_budget_left_error : public std::runtime_error {
	public:
		explicit no_duty_cycle_budget_left_error(const std::string& arg) : std::runtime_error(arg) {}
	};


	/**	 
	 * Budget calculations with regard to the duty cycle.
	 */
	class DutyCycle {

		friend class SystemTests;

		public:			
			DutyCycle();

			/**			 
			 * @param period Number of time slots to consider when computing the duty cycle.
			 * @param max_duty_cycle Maximum duty cycle as a percentage.
			 * @param min_num_supported_pp_links Minimum number of PP links that must be supported.
			 */
			DutyCycle(unsigned int period, double max_duty_cycle, unsigned int min_num_supported_pp_links);

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

			unsigned int getMinNumSupportedPPLinks() const;

			/** Set the strategy used to compute the available duty cycle for a new link. */
			void setStrategy(const DutyCycleBudgetStrategy& strategy);
			DutyCycleBudgetStrategy getStrategy() const;

			/**			 
			 * @param used_budget <used PP duty cycle budget per link>
			 * @param timeouts <timeout in slots per PP link>
			 * @param used_sh_budget used SH duty cycle budget
			 * @param sh_slot_offset offset until next SH channel access
			 * @return <Minimum slot offset, Minimum number of time slots in-between two transmission bursts so that the duty cycle budget is maintained>
			 */
			std::pair<int, int> getPeriodicityPP(std::vector<double> used_pp_budgets, std::vector<int> timeouts, double used_sh_budget, int sh_slot_offset) const;						
			double getSHBudget(const std::vector<double>& used_budget) const;			
			int getOffsetSH(const std::vector<double>& used_budget) const;			
			double getTotalBudget() const;						

		protected:
			std::pair<int, int> getPeriodicityPP_STATIC(std::vector<double> used_pp_budgets, std::vector<int> timeouts, double used_sh_budget, int sh_slot_offset) const;
			std::pair<int, int> getPeriodicityPP_DYNAMIC(std::vector<double> used_pp_budgets, std::vector<int> timeouts, double used_sh_budget, int sh_slot_offset) const;
			double getSHBudget_STATIC(const std::vector<double>& used_budget) const;
			double getSHBudget_DYNAMIC(const std::vector<double>& used_budget) const;			

		protected:
			/** Number of time slots to consider when computing the duty cycle. */
			unsigned int period;
			/** Maximum duty cycle as a percentage. */
			double max_duty_cycle;
			unsigned int min_num_supported_pp_links = 1;
			MovingAverage duty_cycle;						
			DutyCycleBudgetStrategy strategy = DutyCycleBudgetStrategy::STATIC;
	};	
}


#endif