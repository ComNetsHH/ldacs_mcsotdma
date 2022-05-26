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

unsigned int DutyCycle::getPeriodicity() const {
	assert(mac != nullptr && "MAC unset in DutyCycle!");
	
}