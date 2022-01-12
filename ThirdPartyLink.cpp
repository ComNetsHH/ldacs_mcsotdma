//
// Created by Sebastian Lindner on 01/12/22.
//

#include "ThirdPartyLink.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ThirdPartyLink::ThirdPartyLink(const MacId& id_link_initiator, const MacId& id_link_recipient) : id_link_initiator(id_link_initiator), id_link_recipient(id_link_recipient), reservation_map() {}

void ThirdPartyLink::onSlotStart() {
	reservation_map.onSlotStart();
}