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
	this->min_num_supported_pp_links = std::max(uint(1), n);
}

std::pair<int, int> DutyCycle::getPeriodicityPP(std::vector<double> used_pp_budgets, std::vector<int> timeouts, double used_sh_budget, int sh_slot_offset) const {
	switch (this->strategy) {
		case DutyCycleBudgetStrategy::STATIC: {return this->getPeriodicityPP_STATIC(used_pp_budgets, timeouts, used_sh_budget, sh_slot_offset); break;}
		case DutyCycleBudgetStrategy::DYNAMIC: {return this->getPeriodicityPP_DYNAMIC(used_pp_budgets, timeouts, used_sh_budget, sh_slot_offset); break;}
		default: {throw std::runtime_error("unexpected DutyCycle strategy: " + std::to_string(this->strategy));}
	}
}

std::pair<int, int> DutyCycle::getPeriodicityPP_STATIC(std::vector<double> used_pp_budgets, std::vector<int> timeouts, double used_sh_budget, int sh_slot_offset) const {
	// compute statically available budget
	double avail_budget = this->max_duty_cycle / ((double) this->getMinNumSupportedPPLinks() + 1);  // +1 due to Shared Channel
	// translate budget to minimum period n, where periodicity is every second burst of 5*2^n => 10*2^n
	unsigned int min_period = std::max(0.0, std::ceil(std::log2(1.0/(10.0*avail_budget))));		
	unsigned int min_offset = 0;
	return {min_offset, min_period};
}

std::pair<int, int> DutyCycle::getPeriodicityPP_DYNAMIC(std::vector<double> used_pp_budgets, std::vector<int> timeouts, double used_sh_budget, int sh_slot_offset) const {	
	coutd << "computing duty cycle restriction with used_pp_budgets=[";
	for (auto d : used_pp_budgets)
		coutd << d << ", ";
	coutd << "] and used_sh_budget=" << used_sh_budget << " -> max_duty_cycle=" << max_duty_cycle << " and ";
	// check if current budget allows for new PP link
	double avail_budget = this->max_duty_cycle; 
	// if the SH uses less than its fair share and this is the last PP link (or later)
	size_t num_active_links = used_pp_budgets.size();
	if (num_active_links >= min_num_supported_pp_links - 1 && used_sh_budget < (this->max_duty_cycle / (min_num_supported_pp_links + 1)))
		avail_budget -= (this->max_duty_cycle / (min_num_supported_pp_links + 1)); // maximum - fair share for SH
	else
		avail_budget -= used_sh_budget; 
	coutd << avail_budget << " after SH -> ";
	// reduce by PP budgets
	for (double d : used_pp_budgets)
		avail_budget -= d;			
	coutd << " after " << num_active_links << " PPs -> ";
	int min_offset;
	if (avail_budget >= 0.01) {
		coutd << "sufficient -> ";
		min_offset = 0;
	// else, check at which time the next link times out, and how much budget is available then
	} else {
		coutd << "not sufficient, checking when more budget is available -> ";
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
		// translate budget to minimum period n, where periodicity is every second burst of 5*2^n => 10*2^n
		unsigned int min_period = std::max(0.0, std::ceil(std::log2(1.0/(10.0*avail_budget))));	
		coutd << "min_offset=" << min_offset << " max_budget=" << avail_budget << " -> min_period=" << min_period << " -> ";
		return {min_offset, min_period};
	} else {
		std::stringstream ss;
		ss << "no duty cycle budget is left (" << std::to_string(avail_budget) << ")" << " for " << used_pp_budgets.size() << " used_pp_budgets=[";
		for (auto budget : used_pp_budgets)
			ss << budget << ", ";
		ss << "] and " << timeouts.size() << " timeouts=[";
		for (auto timeout : timeouts)
			ss << timeout << ", ";
		ss << "] used_sh_budget=" << used_sh_budget << " sh_slot_offset=" << sh_slot_offset; 
		throw no_duty_cycle_budget_left_error(ss.str());
	}
}

double DutyCycle::getSHBudget(const std::vector<double>& used_budget) const {
	switch (this->strategy) {
		case DutyCycleBudgetStrategy::STATIC: {return this->getSHBudget_STATIC(used_budget); break;}
		case DutyCycleBudgetStrategy::DYNAMIC: {return this->getSHBudget_DYNAMIC(used_budget); break;}
		default: {throw std::runtime_error("unexpected DutyCycle strategy: " + std::to_string(this->strategy)); break;}
	}
}

double DutyCycle::getSHBudget_STATIC(const std::vector<double>& used_budget) const {	
	// compute statically available budget
	double avail_budget = this->max_duty_cycle / ((double) this->getMinNumSupportedPPLinks() + 1);
	return avail_budget;
}

double DutyCycle::getSHBudget_DYNAMIC(const std::vector<double>& used_budget) const {	
	double avail_budget = this->max_duty_cycle;		
	size_t num_active_pp_links = used_budget.size();	
	for (double d : used_budget) 
		avail_budget -= d;
	if (avail_budget <= 0.01) {
		std::stringstream ss;
		ss << "avail_budget=" << avail_budget << " when computing SH budget after used_budget=[";
		for (auto d : used_budget)
			ss << d << ", ";
		ss << "] and " << num_active_pp_links << " active PP links";
		throw std::runtime_error(ss.str());
	}	
	// if not all PP links have been established yet	
	if (num_active_pp_links < min_num_supported_pp_links)
		avail_budget -= this->max_duty_cycle / ((double) min_num_supported_pp_links + 1); // leave budget to establish next PP link immediately
	if (avail_budget == std::numeric_limits<double>::infinity() || avail_budget == -std::numeric_limits<double>::infinity()) {
		std::stringstream ss;
		ss << "sh_budget=inf for " << used_budget.size() << " used_budget=[";
		for (auto d : used_budget)
			ss << d << ", ";
		ss << "] and " << num_active_pp_links << " active PP links";
		throw std::runtime_error(ss.str());
	}
		
	coutd << "SH duty cycle budget is " << avail_budget*100 << "%/" << this->max_duty_cycle*100 << "% at " << num_active_pp_links << "/" << min_num_supported_pp_links << " active PP links -> ";
	return avail_budget;
}

int DutyCycle::getOffsetSH(const std::vector<double>& used_budget) const {
	// compute available budget	
	double avail_budget = getSHBudget(used_budget);
	int slot_offset = std::max(1.0, 1.0 / avail_budget);	
	return slot_offset;
}

double DutyCycle::getTotalBudget() const {
	return max_duty_cycle;
}

unsigned int DutyCycle::getMinNumSupportedPPLinks() const {
	return this->min_num_supported_pp_links;
}

void DutyCycle::setStrategy(const DutyCycleBudgetStrategy& strategy) {	
	this->strategy = strategy;	
}

DutyCycleBudgetStrategy DutyCycle::getStrategy() const {
	return this->strategy;
}