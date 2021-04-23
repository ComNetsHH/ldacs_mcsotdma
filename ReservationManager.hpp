//
// Created by Sebastian Lindner on 14.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP

#include <cstdint>
#include <map>
#include <queue>
#include "ReservationTable.hpp"
#include "FrequencyChannel.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	/**
	 * For one user, the Reservation Manager provides a wrapper for managing reservation tables for each logical frequency channel.
	 */
	class ReservationManager {

		friend class ReservationManagerTests;

		friend class LinkManagerTests;

		friend class MCSOTDMA_MacTests;

	public:
		/**
		 * Implements a comparison of ReservationTables. One table is 'smaller' than another if it has fewer idle slots.
		 * It is used to construct a priority_queue of the tables.
		 */
		class ReservationTableComparison {
		public:
			bool operator()(ReservationTable* tbl1, ReservationTable* tbl2) {
				return tbl1->getNumIdleSlots() < tbl2->getNumIdleSlots();
			}
		};

	public:
		explicit ReservationManager(uint32_t planning_horizon);

		virtual ~ReservationManager();

		/**
		 * Adds a frequency channel and corresponding reservation table.
		 * @param is_p2p
		 * @param center_frequency
		 * @param bandwidth
		 */
		void addFrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth);

		FrequencyChannel* getFreqChannelByCenterFreq(uint64_t center_frequency);

		FrequencyChannel* getFreqChannelByIndex(size_t index);

		std::vector<FrequencyChannel*>& getP2PFreqChannels();

		std::vector<ReservationTable*>& getP2PReservationTables();

		FrequencyChannel* getFreqChannel(const ReservationTable* table);

		ReservationTable* getReservationTableByIndex(size_t index);

		ReservationTable* getReservationTable(const FrequencyChannel* channel);

		FrequencyChannel* getBroadcastFreqChannel();

		ReservationTable* getBroadcastReservationTable();

		/**
		 * Calls hasControlMessage() function on each ReservationTable.
		 * @param num_slots
		 */
		void update(uint64_t num_slots);

		/**
		 * Fetches current reservations from each ReservationTable.
		 */
		std::vector<std::pair<Reservation, const FrequencyChannel*>> collectCurrentReservations();

		/**
		 * @return Number of frequency channels and corresponding reservation tables that are managed.
		 */
		size_t getNumEntries() const;

		/**
		 * Looks through all P2P reservation tables to find the one with most idle slots, so its complexity is O(n).
		 * @return A pointer to the least utilized reservation table according to its reported number of idle slots.
		 */
		ReservationTable* getLeastUtilizedP2PReservationTable();

		/**
		 * @return A priority_queue of the P2P ReservationTables, so that the least-utilized table lies on top.
		 */
		std::priority_queue<ReservationTable*, std::vector<ReservationTable*>, ReservationManager::ReservationTableComparison> getSortedP2PReservationTables() const;

		/**
		 * @param id
		 * @return For every managed FrequencyChannel, a new ReservationTable is instantiated that contains all TX and TX_CONT reservations owned by 'id'.
		 */
		std::vector<std::pair<FrequencyChannel, ReservationTable*>> getTxReservations(const MacId& id) const;

		void updateTables(const std::vector<std::pair<FrequencyChannel, ReservationTable*>>& reservations);

		/**
		 * Links a ReservationTable for the single transmitter that we have.
		 * Reservations can query this table to see if a particular time slot is already utilized by the transmitter.
		 * @param tx_table
		 */
		void setTransmitterReservationTable(ReservationTable* tx_table);

		/**
		 * Links a ReservationTable for a hardware receiver.
		 * Reservations can query this table to see if a particular time slot is already utilized by any (of possibly several) receiver.
		 * @param rx_table
		 */
		void addReceiverReservationTable(ReservationTable*& rx_table);

		/**
		 * @return Hardware receiver ReservationTables.
		 */
		const std::vector<ReservationTable*>& getRxTables() const;

		/**
		 * @return Hardware transmitter ReservationTable.
		 */
		ReservationTable* getTxTable() const;

	protected:

		/**
		 * Searches through 'p2p_frequency_channels' for one that equals 'other'.
		 * @param other
		 * @return A pointer to the local instance that corresponds to 'other'.
		 */
		FrequencyChannel* matchFrequencyChannel(const FrequencyChannel& other) const;

		/** Number of slots to remember both in the past and in the future. */
		uint32_t planning_horizon;
		/** Keeps frequency channels in the same order as p2p_reservation_tables. */
		std::vector<TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel*> p2p_frequency_channels;
		/** Keeps reservation table in the same order as p2p_frequency_channels. */
		std::vector<TUHH_INTAIRNET_MCSOTDMA::ReservationTable*> p2p_reservation_tables;
		/** Map pointer to index s.t. getReservationTable(ptr) doesn't have to search.. */
		std::map<const FrequencyChannel, size_t> p2p_channel_map;
		/** Maps pointer to index s.t. getFrequencyChannel(ptr) doesn't have to search. */
		std::map<const ReservationTable*, size_t> p2p_table_map;
		/** A single broadcast frequency channel is kept. */
		FrequencyChannel* broadcast_frequency_channel = nullptr;
		/** A single broadcast channel reservation table is kept. */
		ReservationTable* broadcast_reservation_table = nullptr;
		/** A transmitter ReservationTable may be kept, which will be linked to all ReservationTables within this manager. */
		ReservationTable* hardware_tx_table = nullptr;
		/** A number of hardware receiver ReservationTables may be kept, which will be linked to all ReservationTables within this manager. */
		std::vector<ReservationTable*> hardware_rx_tables;
	};

}


#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP
