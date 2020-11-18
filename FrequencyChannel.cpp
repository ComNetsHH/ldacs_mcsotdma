//
// Created by Sebastian Lindner on 14.10.20.
//

#include "FrequencyChannel.hpp"

TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::FrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth)
	: is_p2p(is_p2p), center_frequency(center_frequency), bandwidth(bandwidth), is_blacklisted(false) {}

uint64_t TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::getCenterFrequency() const {
	return this->center_frequency;
}

uint64_t TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::getBandwidth() const {
	return this->bandwidth;
}

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::isPointToPointChannel() const {
	return this->is_p2p;
}

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::isBroadcastChannel() const {
	return !this->isPointToPointChannel();
}

bool
TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::operator==(const TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel& other) const {
	return this->isPointToPointChannel() == other.isPointToPointChannel() && this->getCenterFrequency() == other.getCenterFrequency() && this->getBandwidth() == other.getBandwidth();
}

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::operator<(const TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel& other) const {
	return this->getCenterFrequency() < other.getCenterFrequency();
}

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::operator<=(const TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel& other) const {
	return this->getCenterFrequency() <= other.getCenterFrequency();
}

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::operator>(const TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel& other) const {
	return this->getCenterFrequency() > other.getCenterFrequency();
}

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::operator>=(const TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel& other) const {
	return this->getCenterFrequency() >= other.getCenterFrequency();
}

void TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::setBlacklisted(bool value) {
	this->is_blacklisted = value;
}

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::isBlacklisted() const {
	return this->is_blacklisted;
}




