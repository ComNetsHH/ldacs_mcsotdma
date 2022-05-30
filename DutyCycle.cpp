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

std::pair<int, int> DutyCycle::getPeriodicityPP(std::vector<double> used_budget, std::vector<int> timeouts) const {	
	// check if current budget allows for new PP link
	double avail_budget = this->max_duty_cycle - (this->max_duty_cycle / (min_num_supported_pp_links + 1)); // maximum - fair share for SH
	for (double d : used_budget)
		avail_budget -= d;	
	size_t num_active_pp_links = used_budget.size();
	int min_offset;
	if (avail_budget >= 0.01) {
		min_offset = 0;
	// else, check at which time the next PP link times out, and how much budget is available then
	} else {
		while (avail_budget < 0.01 && !timeouts.empty()) {
			auto it = std::min_element(std::begin(timeouts), std::end(timeouts));
			size_t i = std::distance(std::begin(timeouts), it);
			avail_budget += used_budget.at(i);
			min_offset = (*it) + 1;
			timeouts.erase(it);
			num_active_pp_links--;
		}
	}
	if (avail_budget >= 0.01) {
		double max_budget;
		// if there's more PP links to support after the one being currently established...
		if (num_active_pp_links < min_num_supported_pp_links - 1) {
			// then they should fairly share the remaining budget		
			max_budget = avail_budget / (min_num_supported_pp_links - num_active_pp_links);						
		} else {
			// if there's sufficiently many links, the new one can use the remaining budget
			max_budget = avail_budget;		
		}
		// translate budget to minimum slot offset	
		unsigned int min_period = 1.0/max_budget;	
		return {min_offset, min_period};
	} else
		throw no_duty_cycle_budget_left_error("no duty cycle budget is left");
}

int DutyCycle::getPeriodicitySH(std::vector<double> used_budget, std::vector<int> timeouts) const {
	
}