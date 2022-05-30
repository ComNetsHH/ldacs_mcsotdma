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

unsigned int DutyCycle::getPeriodicityPP(std::vector<double> pp_duty_cycle_contribs) const {	
	double used_budget = 0.0;
	for (double d : pp_duty_cycle_contribs)
		used_budget += d;
	std::cout << "used_budget=" << used_budget << std::endl;
	size_t num_active_pp_links = pp_duty_cycle_contribs.size();
	double remaining_budget = max_duty_cycle - used_budget;
	std::cout << "remaining_budget=" << remaining_budget << std::endl;
	// PP channel access must leave a fair share for the SH	
	remaining_budget -= this->max_duty_cycle / (min_num_supported_pp_links + 1); // hence do #PPLinks + 1
	std::cout << "remaining_budget=" << remaining_budget << std::endl;
	if (remaining_budget <= 0.001)
		throw no_duty_cycle_budget_left_error("no duty cycle budget is left");
	// determine maximum budget that can currently be used
	double max_budget;
	// if there's more PP links to support after the one being currently established...
	if (num_active_pp_links < min_num_supported_pp_links - 1) {
		// then they should fairly share the remaining budget		
		max_budget = remaining_budget / (min_num_supported_pp_links - num_active_pp_links);				
		std::cout << "more links to support" << std::endl;
	} else {
		// if there's sufficiently many links, the new one can use the remaining budget
		max_budget = remaining_budget;
		std::cout << "last link" << std::endl;
	}
	// translate budget to minimum slot offset
	std::cout << "max_budget=" << max_budget << std::endl;
	unsigned int min_offset = 1.0/max_budget;
	std::cout << "min_offset=" << min_offset << " " << 1.0/0.02 << std::endl;
	return min_offset;
}