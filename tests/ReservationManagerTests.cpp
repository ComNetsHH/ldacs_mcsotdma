//
// Created by Sebastian Lindner on 14.10.20.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ReservationTable.hpp"
#include "../ReservationManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class ReservationManagerTests : public CppUnit::TestFixture {
		private:
			uint32_t planning_horizon = 1024;
			ReservationManager* reservation_manager;
		
		public:
			void setUp() override {
				reservation_manager = new ReservationManager(planning_horizon);
			}
			
			void tearDown() override {
				delete reservation_manager;
			}
			
			void testAddFreqChannel() {
				bool p2p_channel = true;
				uint64_t center_freq = 1000;
				uint64_t bandwidth = 500;
				
				// Fetching it now should throw an exception.
				bool exception_thrown = false;
				try {
					const FrequencyChannel* channel = reservation_manager->getFreqChannelByIndex(0);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(true, exception_thrown);
				
				// Add it.
				reservation_manager->addFrequencyChannel(p2p_channel, center_freq, bandwidth);
				
				// Now we should be able to get both reservation table and frequency channel.
				exception_thrown = false;
				try {
					const FrequencyChannel* channel = reservation_manager->getFreqChannelByIndex(0);
					const ReservationTable* table = reservation_manager->getReservationTableByIndex(0);
				} catch (const std::exception& e) {
					exception_thrown = true;
				}
				CPPUNIT_ASSERT_EQUAL(false, exception_thrown);
			}
			
			void testUpdate() {
				bool p2p_channel = true;
				uint64_t center_freq1 = 1000, center_freq2 = center_freq1 + 1;
				uint64_t bandwidth = 500;
				reservation_manager->addFrequencyChannel(p2p_channel, center_freq1, bandwidth);
				reservation_manager->addFrequencyChannel(p2p_channel, center_freq2, bandwidth);
				Timestamp now = Timestamp();
				CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTableByIndex(0)->getCurrentSlot() == now);
				CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTableByIndex(1)->getCurrentSlot() == now);
				
				uint64_t num_slots = 5;
				reservation_manager->update(num_slots);
				CPPUNIT_ASSERT_EQUAL(false,
				                     reservation_manager->getReservationTableByIndex(0)->getCurrentSlot() == now);
				CPPUNIT_ASSERT_EQUAL(false,
				                     reservation_manager->getReservationTableByIndex(1)->getCurrentSlot() == now);
				
				now += num_slots;
				
				CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTableByIndex(0)->getCurrentSlot() == now);
				CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getReservationTableByIndex(1)->getCurrentSlot() == now);
			}
			
			void testGetLeastUtilizedReservationTable() {
				bool p2p_channel = true;
				uint64_t center_freq1 = 1000, center_freq2 = center_freq1 + 1;
				uint64_t bandwidth = 500;
				reservation_manager->addFrequencyChannel(p2p_channel, center_freq1, bandwidth);
				reservation_manager->addFrequencyChannel(p2p_channel, center_freq2, bandwidth);
				
				ReservationTable* table1 = reservation_manager->getReservationTableByIndex(0);
				ReservationTable* table2 = reservation_manager->getReservationTableByIndex(1);
				
				// Mark one slot as busy in table1.
				table1->mark(0, Reservation(MacId(0), Reservation::Action::BUSY));
				ReservationTable* least_utilized_table = reservation_manager->getLeastUtilizedP2PReservationTable();
				CPPUNIT_ASSERT_EQUAL(table2, least_utilized_table); // table2 contains more idle slots now.
				
				// Now mark *two* slots busy in table2.
				table2->mark(0, Reservation(MacId(0), Reservation::Action::BUSY));
				table2->mark(1, Reservation(MacId(0), Reservation::Action::BUSY));
				least_utilized_table = reservation_manager->getLeastUtilizedP2PReservationTable();
				CPPUNIT_ASSERT_EQUAL(table1, least_utilized_table); // table1 contains more idle slots now.
			}
			
			void testGetSortedReservationTables() {
				// Should get an empty queue if no tables are present.
				CPPUNIT_ASSERT_EQUAL(true, reservation_manager->getSortedP2PReservationTables().empty());
				// Add reservation tables.
				bool p2p_channel = true;
				uint64_t center_freq1 = 1000, center_freq2 = center_freq1 + 1, center_freq3 = center_freq2 + 1;
				uint64_t bandwidth = 500;
				reservation_manager->addFrequencyChannel(p2p_channel, center_freq1, bandwidth);
				reservation_manager->addFrequencyChannel(p2p_channel, center_freq2, bandwidth);
				reservation_manager->addFrequencyChannel(p2p_channel, center_freq3, bandwidth);
				
				ReservationTable* table1 = reservation_manager->getReservationTableByIndex(0); // No busy slots.
				ReservationTable* table2 = reservation_manager->getReservationTableByIndex(1); // Three busy slots.
				table2->mark(0, Reservation(MacId(0), Reservation::Action::BUSY));
				table2->mark(1, Reservation(MacId(0), Reservation::Action::BUSY));
				table2->mark(2, Reservation(MacId(0), Reservation::Action::BUSY));
				ReservationTable* table3 = reservation_manager->getReservationTableByIndex(2); // Two busy slots.
				table3->mark(0, Reservation(MacId(0), Reservation::Action::BUSY));
				table3->mark(1, Reservation(MacId(0), Reservation::Action::BUSY));
				
				auto queue = reservation_manager->getSortedP2PReservationTables();
				ReservationTable* table = queue.top();
				CPPUNIT_ASSERT(table == table1);
				queue.pop();
				table = queue.top();
				CPPUNIT_ASSERT(table == table3);
				queue.pop();
				table = queue.top();
				CPPUNIT_ASSERT(table == table2);
				queue.pop();
				CPPUNIT_ASSERT_EQUAL(true, queue.empty());
			}
			
			void testGetByPointer() {
				uint64_t freq1 = 1000, freq2 = 2000, bc_freq = 3000, bandwidth = 500;
				reservation_manager->addFrequencyChannel(true, freq1, bandwidth);
				reservation_manager->addFrequencyChannel(true, freq2, bandwidth);
				reservation_manager->addFrequencyChannel(false, bc_freq, bandwidth);
				FrequencyChannel* channel1 = reservation_manager->getFreqChannelByIndex(0),
						* channel2 = reservation_manager->getFreqChannelByIndex(1),
						* bc_channel = reservation_manager->broadcast_frequency_channel;
				ReservationTable* table1 = reservation_manager->getReservationTableByIndex(0),
						* table2 = reservation_manager->getReservationTableByIndex(1),
						* bc_table = reservation_manager->broadcast_reservation_table;
				CPPUNIT_ASSERT(channel1 == reservation_manager->getFreqChannel(table1));
				CPPUNIT_ASSERT(channel2 == reservation_manager->getFreqChannel(table2));
				CPPUNIT_ASSERT(bc_channel == reservation_manager->getFreqChannel(bc_table));
				
				CPPUNIT_ASSERT(table1 == reservation_manager->getReservationTable(channel1));
				CPPUNIT_ASSERT(table2 == reservation_manager->getReservationTable(channel2));
				CPPUNIT_ASSERT(bc_table == reservation_manager->getReservationTable(bc_channel));
			}
			
			void testGetTxReservations() {
				uint64_t freq1 = 1000, freq2 = 2000, bc_freq = 3000, bandwidth = 500;
				reservation_manager->addFrequencyChannel(true, freq1, bandwidth);
				reservation_manager->addFrequencyChannel(true, freq2, bandwidth);
				reservation_manager->addFrequencyChannel(false, bc_freq, bandwidth);
				
				ReservationTable *tbl1 = reservation_manager->getReservationTableByIndex(0),
						  		 *tbl2 = reservation_manager->getReservationTableByIndex(1),
								 *bc_tbl = reservation_manager->broadcast_reservation_table;
				
				MacId id = MacId(42), other_id = MacId(id.getId() + 1);
				unsigned int o11 = 2, o12 = 5, o13 = 12;
				tbl1->mark(o11, Reservation(id, Reservation::TX));
				tbl1->mark(o12, Reservation(id, Reservation::TX));
				tbl1->mark(o13, Reservation(id, Reservation::TX));
				tbl1->mark(o11 + 1, Reservation(other_id, Reservation::TX));
				tbl1->mark(o12 + 1, Reservation(other_id, Reservation::TX));
				tbl1->mark(o13 + 1, Reservation(other_id, Reservation::TX));
				
				unsigned int o21 = 12, o22 = 14, o23 = 16;
				tbl2->mark(o21, Reservation(id, Reservation::TX));
				tbl2->mark(o22, Reservation(id, Reservation::TX));
				tbl2->mark(o23, Reservation(id, Reservation::TX));
				tbl2->mark(o21 + 1, Reservation(other_id, Reservation::TX));
				tbl2->mark(o22 + 1, Reservation(other_id, Reservation::TX));
				tbl2->mark(o23 + 1, Reservation(other_id, Reservation::TX));
				
				unsigned int o31 = 1, o32 = 15, o33 = 19;
				bc_tbl->mark(o31, Reservation(id, Reservation::TX));
				bc_tbl->mark(o32, Reservation(id, Reservation::TX));
				bc_tbl->mark(o33, Reservation(id, Reservation::TX));
				bc_tbl->mark(o31 + 1, Reservation(other_id, Reservation::TX));
				bc_tbl->mark(o32 + 1, Reservation(other_id, Reservation::TX));
				bc_tbl->mark(o33 + 1, Reservation(other_id, Reservation::TX));
				
				auto local_reservations = reservation_manager->getTxReservations(id);
				for (const auto& pair : local_reservations) {
					const FrequencyChannel& channel = pair.first;
					const ReservationTable* table = pair.second;
					// First channel
					if (channel.getCenterFrequency() == freq1) {
						for (int i = 0; i < 50; i++) {
							// Should have reservations where we marked them
							if (i == o11 || i == o12 || i == o13) {
								CPPUNIT_ASSERT_EQUAL(table->getReservation(i).getTarget(), id);
								CPPUNIT_ASSERT_EQUAL(table->getReservation(i).getAction(), Reservation::Action::TX);
							// all others should be default
							} else
								CPPUNIT_ASSERT(table->getReservation(i) == Reservation());
						}
					} else if (channel.getCenterFrequency() == freq2) {
						for (int i = 0; i < 50; i++) {
							if (i == o21 || i == o22 || i == o23) {
								CPPUNIT_ASSERT_EQUAL(table->getReservation(i).getTarget(), id);
								CPPUNIT_ASSERT_EQUAL(table->getReservation(i).getAction(), Reservation::Action::TX);
							} else
								CPPUNIT_ASSERT(table->getReservation(i) == Reservation());
						}
					} else if (channel.getCenterFrequency() == bc_freq) {
						for (int i = 0; i < 50; i++) {
							if (i == o31 || i == o32 || i == o33) {
								CPPUNIT_ASSERT_EQUAL(table->getReservation(i).getTarget(), id);
								CPPUNIT_ASSERT_EQUAL(table->getReservation(i).getAction(), Reservation::Action::TX);
							} else
								CPPUNIT_ASSERT(table->getReservation(i) == Reservation());
						}
					}
					delete table;
				}
			}
			
			void testUpdateTables() {
				uint64_t freq1 = 1000, freq2 = 2000, bc_freq = 3000, bandwidth = 500;
				reservation_manager->addFrequencyChannel(true, freq1, bandwidth);
				reservation_manager->addFrequencyChannel(true, freq2, bandwidth);
				reservation_manager->addFrequencyChannel(false, bc_freq, bandwidth);
				
				ReservationTable *tbl1 = reservation_manager->getReservationTableByIndex(0),
								 *tbl2 = reservation_manager->getReservationTableByIndex(1);
				MacId id = MacId(42);
				Reservation reservation = Reservation(id, Reservation::Action::TX);
				tbl1->mark(12, reservation);
				tbl2->mark(5, reservation);
				
				ReservationManager other_manager = ReservationManager(planning_horizon);
				other_manager.addFrequencyChannel(true, freq1, bandwidth);
				other_manager.addFrequencyChannel(true, freq2, bandwidth);
				other_manager.addFrequencyChannel(false, bc_freq, bandwidth);
				const ReservationTable *remote_tbl1 = other_manager.getReservationTableByIndex(0),
										*remote_tbl2 = other_manager.getReservationTableByIndex(1);
				CPPUNIT_ASSERT(*remote_tbl1 != *tbl1);
				CPPUNIT_ASSERT(*remote_tbl2 != *tbl2);
				
				auto local_reservations = reservation_manager->getTxReservations(id);
				other_manager.updateTables(local_reservations);
				CPPUNIT_ASSERT(*remote_tbl1 == *tbl1);
				CPPUNIT_ASSERT(*remote_tbl2 == *tbl2);
				for (const auto& pair : local_reservations)
					delete pair.second;
			}
			
			void testCollectCurrentReservations() {
				reservation_manager->addFrequencyChannel(false, 1000, 500);
				reservation_manager->addFrequencyChannel(true, 2000, 500);
				reservation_manager->reservation_tables.at(0)->mark(1, Reservation(MacId(42), Reservation::TX));
				reservation_manager->broadcast_reservation_table->mark(1, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX));
				reservation_manager->update(1);
				auto reservations = reservation_manager->collectCurrentReservations();
				CPPUNIT_ASSERT_EQUAL(size_t(2), reservations.size());
				CPPUNIT_ASSERT_EQUAL(SYMBOLIC_LINK_ID_BROADCAST, reservations.at(0).first.getTarget());
				CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, reservations.at(0).first.getAction());
				CPPUNIT_ASSERT_EQUAL(MacId(42), reservations.at(1).first.getTarget());
				CPPUNIT_ASSERT_EQUAL(Reservation::Action::TX, reservations.at(1).first.getAction());
			}
		
		CPPUNIT_TEST_SUITE(ReservationManagerTests);
			CPPUNIT_TEST(testAddFreqChannel);
			CPPUNIT_TEST(testUpdate);
			CPPUNIT_TEST(testGetLeastUtilizedReservationTable);
			CPPUNIT_TEST(testGetSortedReservationTables);
			CPPUNIT_TEST(testGetByPointer);
			CPPUNIT_TEST(testGetTxReservations);
			CPPUNIT_TEST(testUpdateTables);
			CPPUNIT_TEST(testCollectCurrentReservations);
		CPPUNIT_TEST_SUITE_END();
	};
}