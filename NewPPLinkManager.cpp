//
// Created by Sebastian Lindner on 12/21/21.
//

#include "NewPPLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

void onReceptionBurstStart(unsigned int burst_length) {

}

void onReceptionBurst(unsigned int remaining_burst_length) {

}

L2Packet* onTransmissionBurstStart(unsigned int burst_length) {
	return nullptr;
}

void onTransmissionBurst(unsigned int remaining_burst_length) {

}

void notifyOutgoing(unsigned long num_bits) {

}

void onSlotStart(uint64_t num_slots) {

}

void onSlotEnd() {

}