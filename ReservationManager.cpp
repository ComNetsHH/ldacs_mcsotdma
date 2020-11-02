//
// Created by Sebastian Lindner on 14.10.20.
//

#include "ReservationManager.hpp"

TUHH_INTAIRNET_MCSOTDMA::ReservationManager::ReservationManager(uint32_t planning_horizon) : planning_horizon(planning_horizon), reservation_tables() {

}

void TUHH_INTAIRNET_MCSOTDMA::ReservationManager::addFrequencyChannel(bool is_point_to_point_channel,
                                                                      uint64_t center_frequency, uint64_t bandwidth) {
//	reservation_tables.insert(std::pair<FrequencyChannel, ReservationTable>(FrequencyChannel(is_point_to_point_channel, center_frequency, bandwidth), ReservationTable(this->planning_horizon)));
}
