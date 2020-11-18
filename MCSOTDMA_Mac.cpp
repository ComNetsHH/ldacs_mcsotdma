//
// Created by Sebastian Lindner on 16.11.20.
//

#include "MCSOTDMA_Mac.hpp"

void TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::notifyOutgoing(unsigned int num_bits,
                                                           const TUHH_INTAIRNET_MCSOTDMA::MacId& mac_id) {
	
}

void TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::passToLower(TUHH_INTAIRNET_MCSOTDMA::L2Packet* packet) {

}

bool TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::shouldLinkBeArqProtected(const TUHH_INTAIRNET_MCSOTDMA::MacId& mac_id) const {
	if (this->upper_layer == nullptr)
		throw std::runtime_error("Cannot query the ARQ sublayer because it has not been set yet (nullptr).");
	return this->upper_layer->shouldLinkBeArqProtected(mac_id);
}

TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::MCSOTDMA_Mac(TUHH_INTAIRNET_MCSOTDMA::ReservationManager& reservation_manager)
	: reservation_manager(reservation_manager) {
	
}
