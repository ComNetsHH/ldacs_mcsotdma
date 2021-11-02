//
// Created by seba on 4/21/21.
//

#include "LinkInfo.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkInfo::LinkInfo(const MacId& tx_id, const MacId& rx_id, uint64_t p2p_channel_center_freq, int offset, unsigned int timeout, unsigned int burst_length, unsigned int burst_length_tx)
	: tx_id(tx_id), rx_id(rx_id), p2p_channel_center_freq(p2p_channel_center_freq), offset(offset), timeout(timeout), burst_length(burst_length), burst_length_tx(burst_length_tx) {}

LinkInfo::LinkInfo() : LinkInfo(SYMBOLIC_ID_UNSET, SYMBOLIC_ID_UNSET, 0, 0, 0, 0, 0) {}

LinkInfo::LinkInfo(const LinkInfo& other) : tx_id(other.tx_id), rx_id(other.rx_id), p2p_channel_center_freq(other.p2p_channel_center_freq), offset(other.offset), timeout(other.timeout), burst_length(other.burst_length), burst_length_tx(other.burst_length_tx) {}

const MacId& LinkInfo::getTxId() const {
	return tx_id;
}

const MacId& LinkInfo::getRxId() const {
	return rx_id;
}

uint64_t LinkInfo::getP2PChannelCenterFreq() const {
	return p2p_channel_center_freq;
}

int LinkInfo::getOffset() const {
	return offset;
}

unsigned int LinkInfo::getTimeout() const {
	return timeout;
}

unsigned int LinkInfo::getBurstLength() const {
	return burst_length;
}

unsigned int LinkInfo::getBurstLengthTx() const {
	return burst_length_tx;
}

unsigned int LinkInfo::getBits() const {
	return tx_id.getBits() + rx_id.getBits() + 5*8;
}

void LinkInfo::setHasExpired(bool flag) {
	this->has_expired = flag;
}

bool LinkInfo::hasExpired() const {
	return this->has_expired;
}