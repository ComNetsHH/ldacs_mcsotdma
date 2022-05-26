#include "DutyCycle.hpp"
#include "MCSOTDMA_Mac.hpp"
#include <cassert>

using namespace TUHH_INTAIRNET_MCSOTDMA;

DutyCycle::DutyCycle(unsigned int period, double max_duty_cycle, MCSOTDMA_Mac *mac) : period(period), max_duty_cycle(max_duty_cycle), duty_cycle(MovingAverage(period)), mac(mac) {}

DutyCycle::DutyCycle() : DutyCycle(100, 0.1, nullptr) {}

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

unsigned int DutyCycle::getPeriodicity(bool sh_channel_access) const {
	assert(mac != nullptr && "MAC unset in DutyCycle!");
	auto num_txs_and_num_active_pp_links = mac->getDutyCycleContributions();
	double &num_txs_per_time_slot = num_txs_and_num_active_pp_links.first;
	size_t &num_active_pp_links = num_txs_and_num_active_pp_links.second;	
	double used_budget = num_txs_per_time_slot * ((double) period);
	double remaining_budget = max_duty_cycle - used_budget;
	// PP channel access must leave a fair share for the SH
	if (!sh_channel_access)
		remaining_budget -= this->max_duty_cycle / (min_num_supported_pp_links + 1); // hence do #PPLinks + 1
	// determine maximum budget that can currently be used
	double max_budget;
	// if there's more PP links to support after the one being currently established...
	if (num_active_pp_links < min_num_supported_pp_links - 1) {
		// then they should fairly share the remaining budget		
		max_budget = remaining_budget / (min_num_supported_pp_links - num_active_pp_links);		
		coutd << remaining_budget << " / (" << min_num_supported_pp_links << " - " << num_active_pp_links << ") = " << max_budget << std::endl;
	} else {
		// if there's sufficiently many links, the new one can use the remaining budget
		max_budget = remaining_budget;
	}
	// translate budget to minimum slot offset
	unsigned int min_offset = 1.0/max_budget;
	return min_offset;
}