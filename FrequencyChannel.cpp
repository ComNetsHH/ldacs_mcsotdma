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

#include "FrequencyChannel.hpp"

TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::FrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth)
		: is_p2p(is_p2p), center_frequency(center_frequency), bandwidth(bandwidth), is_blacklisted(false) {}

TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::FrequencyChannel(const TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel& other)
		: FrequencyChannel(other.is_p2p, other.center_frequency, other.bandwidth) {
	is_blacklisted = other.is_blacklisted;
}

uint64_t TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::getCenterFrequency() const {
	return this->center_frequency;
}

uint64_t TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::getBandwidth() const {
	return this->bandwidth;
}

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::isPP() const {
	return this->is_p2p;
}

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::isSH() const {
	return !this->isPP();
}

bool
TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::operator==(const TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel& other) const {
	return this->isPP() == other.isPP() && this->getCenterFrequency() == other.getCenterFrequency() && this->getBandwidth() == other.getBandwidth();
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

bool TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::isBlocked() const {
	return this->is_blacklisted;
}

bool
TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel::operator!=(const TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel& other) const {
	return !(*this == other);
}




