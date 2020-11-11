//
// Created by Sebastian Lindner on 11.11.20.
//

#include "L2Layer.hpp"

TUHH_INTAIRNET_MCSOTDMA::L2Layer::L2Layer(uint32_t reservation_planning_horizon)
	: queue_manager(new QueueManager()), reservation_manager(new ReservationManager(reservation_planning_horizon)) {
	queue_manager->setReservationManager(reservation_manager);
}

TUHH_INTAIRNET_MCSOTDMA::L2Layer::~L2Layer() {
	delete queue_manager;
	delete reservation_manager;
}
