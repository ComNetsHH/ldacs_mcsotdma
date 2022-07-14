#include "DutyCycle.hpp"
#include "MCSOTDMA_Mac.hpp"
#include <cassert>

using namespace TUHH_INTAIRNET_MCSOTDMA;

DutyCycle::DutyCycle(unsigned int period, double max_duty_cycle, unsigned int min_num_supported_pp_links) : period(period), max_duty_cycle(max_duty_cycle), duty_cycle(MovingAverage(period)), min_num_supported_pp_links(min_num_supported_pp_links) {}

DutyCycle::DutyCycle() : DutyCycle(100, 0.1, 4) {}

void DutyCycle::reportNumTransmissions(unsigned int num_txs) {
	duty_cycle.put(num_txs);	
}

bool DutyCycle::shouldEmitStatistic() const {
	return duty_cycle.hasReachedNumValues();
}

double DutyCycle::get() const {
	return duty_cycle.get();
}

void DutyCycle::setMinNumSupportedPPLinks(unsigned int n) {
	this->min_num_supported_pp_links = n;
}

std::pair<int, int> DutyCycle::getPeriodicityPP(std::vector<double> used_pp_budgets, std::vector<int> timeouts, double used_sh_budget, int sh_slot_offset) const {	
	// coutd << "computing duty cycle restriction with " << used_pp_budgets.size() << " PPs and " << used_sh_budget << " used by SH -> ";
	// check if current budget allows for new PP link
	double avail_budget = this->max_duty_cycle; 
	// if the SH uses less than its fair share
	if (used_sh_budget < (this->max_duty_cycle / (min_num_supported_pp_links + 1)))
		avail_budget -= (this->max_duty_cycle / (min_num_supported_pp_links + 1)); // maximum - fair share for SH
	else
		avail_budget -= used_sh_budget; 
	// coutd << avail_budget << " after SH -> ";
	// reduce by PP budgets
	for (double d : used_pp_budgets)
		avail_budget -= d;	
	// coutd << avail_budget << " after ";
	size_t num_active_links = used_pp_budgets.size();
	// coutd << num_active_links << " PPs -> ";
	int min_offset;
	if (avail_budget >= 0.01) {
		// coutd << "sufficient -> ";
		min_offset = 0;
	// else, check at which time the next link times out, and how much budget is available then
	} else {
		// coutd << "not sufficient, checking when more budget is available -> ";
		// add the SH channel access
		bool sh_timeout_present;	
		if (sh_slot_offset >= 0)	{		
			timeouts.push_back(sh_slot_offset);
			used_pp_budgets.push_back(used_sh_budget);
			sh_timeout_present = true;
		} else
			sh_timeout_present = false;
		while (avail_budget < 0.01 && !timeouts.empty()) {
			auto it = std::min_element(std::begin(timeouts), std::end(timeouts));
			size_t i = std::distance(std::begin(timeouts), it);
			avail_budget += used_pp_budgets.at(i);
			min_offset = (*it) + 1;			
			if (sh_timeout_present) {
				if (it == timeouts.end() - 1) {
					sh_timeout_present = false;
				} else
					num_active_links--;
			} else
				num_active_links--;
			timeouts.erase(it);
			used_pp_budgets.erase(used_pp_budgets.begin() + i);
		}
	}
	if (avail_budget >= 0.01) {
		// coutd << "arrived at " << avail_budget << " -> ";		
		// // if there's more PP links to support after the one being currently established...
		// if (num_active_links < min_num_supported_pp_links - 1) {
		// 	// then they should fairly share the remaining budget		
		// 	max_budget = avail_budget / (min_num_supported_pp_links - num_active_links);						
		// } else {
		// 	// if there's sufficiently many links, the new one can use the remaining budget
		// 	max_budget = avail_budget;		
		// }		
		// translate budget to minimum period n, where periodicity is every second burst of 5*2^n => 10*2^n
		unsigned int min_period = std::max(0.0, std::ceil(std::log2(1.0/(10.0*avail_budget))));	
		// coutd << "max_budget=" << avail_budget << " -> min_period=" << min_period << " -> ";
		return {min_offset, min_period};
	} else
		throw no_duty_cycle_budget_left_error("no duty cycle budget is left");
}

double DutyCycle::getSHBudget(const std::vector<double>& used_budget) const {	
	double avail_budget = this->max_duty_cycle;			
	for (double d : used_budget) 
		avail_budget -= d;				
	// if not all PP links have been established yet
	size_t num_active_pp_links = used_budget.size();
	if (num_active_pp_links < min_num_supported_pp_links)
		avail_budget -= this->max_duty_cycle / ((double) min_num_supported_pp_links + 1); // leave budget to establish next PP link immediately
	return avail_budget;
}

int DutyCycle::getOffsetSH(const std::vector<double>& used_budget) const {
	// compute available budget	
	double avail_budget = getSHBudget(used_budget);
	int slot_offset = std::max(1.0, 1.0 / avail_budget);	
	return slot_offset;
}