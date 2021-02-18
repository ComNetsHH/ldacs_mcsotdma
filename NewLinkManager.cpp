//
// Created by seba on 2/18/21.
//

#include "NewLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

NewLinkManager::NewLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac)
		: link_id(link_id), reservation_manager(reservation_manager), mac(mac),
		  link_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */,
		  random_device(new std::random_device), generator((*random_device)()) {}

NewLinkManager::~NewLinkManager() {
	delete random_device;
}

