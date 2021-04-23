//
// Created by seba on 4/21/21.
//

#include "LinkInfo.hpp"

TUHH_INTAIRNET_MCSOTDMA::LinkInfo::LinkInfo(const TUHH_INTAIRNET_MCSOTDMA::MacId& tx_id, const TUHH_INTAIRNET_MCSOTDMA::MacId& rx_id, uint64_t p2p_channel_center_freq, int offset, unsigned int timeout, unsigned int burst_length, unsigned int burst_length_tx)
	: tx_id(tx_id), rx_id(rx_id), p2p_channel_center_freq(p2p_channel_center_freq), offset(offset), timeout(timeout), burst_length(burst_length), burst_length_tx(burst_length_tx) {}

TUHH_INTAIRNET_MCSOTDMA::LinkInfo::LinkInfo() : LinkInfo(SYMBOLIC_ID_UNSET, SYMBOLIC_ID_UNSET, 0, 0, 0, 0, 0) {}

TUHH_INTAIRNET_MCSOTDMA::LinkInfo::LinkInfo(const TUHH_INTAIRNET_MCSOTDMA::LinkInfo& other) : tx_id(other.tx_id), rx_id(other.rx_id), p2p_channel_center_freq(other.p2p_channel_center_freq), offset(other.offset), timeout(other.timeout), burst_length(other.burst_length), burst_length_tx(other.burst_length_tx) {}

const TUHH_INTAIRNET_MCSOTDMA::MacId& TUHH_INTAIRNET_MCSOTDMA::LinkInfo::getTxId() const {
	return tx_id;
}

const TUHH_INTAIRNET_MCSOTDMA::MacId& TUHH_INTAIRNET_MCSOTDMA::LinkInfo::getRxId() const {
	return rx_id;
}

uint64_t TUHH_INTAIRNET_MCSOTDMA::LinkInfo::getP2PChannelCenterFreq() const {
	return p2p_channel_center_freq;
}

int TUHH_INTAIRNET_MCSOTDMA::LinkInfo::getOffset() const {
	return offset;
}

unsigned int TUHH_INTAIRNET_MCSOTDMA::LinkInfo::getTimeout() const {
	return timeout;
}

unsigned int TUHH_INTAIRNET_MCSOTDMA::LinkInfo::getBurstLength() const {
	return burst_length;
}

unsigned int TUHH_INTAIRNET_MCSOTDMA::LinkInfo::getBurstLengthTx() const {
	return burst_length_tx;
}

unsigned int TUHH_INTAIRNET_MCSOTDMA::LinkInfo::getBits() const {
	return tx_id.getBits() + rx_id.getBits() + 5*8;
}
