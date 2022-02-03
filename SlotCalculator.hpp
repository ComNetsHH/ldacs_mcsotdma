#ifndef TUHH_INTAIRNET_MC_SOTDMA_SLOTCALCULATOR_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_SLOTCALCULATOR_HPP

namespace TUHH_INTAIRNET_MCSOTDMA::SlotCalculator {

std::pair<std::vector<int>, std::vector<int>> calculateTxRxSlots(const int &start_slot_offset, const int &burst_length, const int &burst_length_tx, const int &burst_length_rx, const int &burst_offset, const int &timeout);

}

#endif // TUHH_INTAIRNET_MC_SOTDMA_SLOTCALCULATOR_HPP