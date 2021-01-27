//
// Created by seba on 1/14/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../LinkManagementEntity.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

    /**
     * The LinkManagementEntity is a module of the LinkManager.
     * As such, it cannot be easily tested on its own. Most tests are put into LinkManagerTests or even SystemTests.
     */
    class LinkManagementEntityTests : public CppUnit::TestFixture {
    private:
        TestEnvironment* env;

        LinkManager* link_manager;
        ReservationManager* reservation_manager;
        MacId own_id;
        MacId communication_partner_id;
        uint32_t planning_horizon;
        uint64_t center_frequency1, center_frequency2, center_frequency3, bc_frequency, bandwidth;
        unsigned long num_bits_going_out = 800*100;
        MACLayer* mac;

        unsigned int tx_timeout = 5, init_offset = 1, tx_offset = 3, num_renewal_attempts = 2;
        LinkManagementEntity* lme;

    public:
        void setUp() override {
            own_id = MacId(42);
            communication_partner_id = MacId(43);
            env = new TestEnvironment(own_id, communication_partner_id);
            planning_horizon = env->planning_horizon;
            center_frequency1 = env->center_frequency1;
            center_frequency2 = env->center_frequency2;
            center_frequency3 = env->center_frequency3;
            bc_frequency = env->bc_frequency;
            bandwidth = env->bandwidth;

            mac = env->mac_layer;
            reservation_manager = mac->reservation_manager;
            link_manager = mac->getLinkManager(communication_partner_id);
            link_manager->assign(reservation_manager->getFreqChannelByCenterFreq(center_frequency1));

            lme = link_manager->lme;
        }

        void tearDown() override {
            delete env;
        }

        void testSchedule() {
            lme->configure(num_renewal_attempts, tx_timeout, init_offset, tx_offset);
            const std::vector<uint64_t>& slots = lme->scheduled_requests;
            CPPUNIT_ASSERT_EQUAL(size_t(num_renewal_attempts), slots.size());
            // Manual check: init offset=1, tx every 3 slots, 5 txs -> tx at [1,4,7,10,13].
            CPPUNIT_ASSERT_EQUAL(uint64_t(10), slots.at(0));
            CPPUNIT_ASSERT_EQUAL(uint64_t(4), slots.at(1));
        }

        void testUpdate() {
//            coutd.setVerbose(true);
            lme->configure(num_renewal_attempts, tx_timeout, init_offset, tx_offset);
            const auto& request_slots = lme->scheduled_requests;
            size_t num_request_triggers = 0;
            while (num_request_triggers < num_renewal_attempts) {
                mac->update(1);
                bool should_send_request = lme->hasControlMessage();
                if (should_send_request) {
                    num_request_triggers++;
                    delete lme->getControlMessage();
                    // Manual check.
                    CPPUNIT_ASSERT(mac->getCurrentSlot() == uint64_t(4) || mac->getCurrentSlot() == uint64_t(10));
                }
            }
            // Once all requests should've been sent, don't request to send another.
            CPPUNIT_ASSERT_EQUAL(false, lme->hasControlMessage());
            mac->update(1);
            CPPUNIT_ASSERT_EQUAL(false, lme->hasControlMessage());
            mac->update(1);
            CPPUNIT_ASSERT_EQUAL(false, lme->hasControlMessage());
            // Should've requested the right number of requests.
            CPPUNIT_ASSERT_EQUAL(size_t(num_renewal_attempts), num_request_triggers);
            CPPUNIT_ASSERT_EQUAL(true, lme->scheduled_requests.empty());
//            coutd.setVerbose(false);
        }

        void testPopulateRequest() {
//            coutd.setVerbose(true);

            L2Packet* request = lme->prepareRequest();
            lme->populateRequest(request);
            auto proposal = (LinkManagementEntity::ProposalPayload*) request->getPayloads().at(1);
            const auto& map = proposal->proposed_resources;
            CPPUNIT_ASSERT_EQUAL(size_t(lme->num_proposed_channels), map.size());

            for (const auto& item1 : map) {
                const FrequencyChannel* channel1 = item1.first;
                const std::vector<unsigned int>& offsets1 = item1.second;
                for (unsigned int slot : offsets1)
                    coutd << "f=" << *channel1 << " t=" << slot << std::endl;
                // Time slots across channels should not be identical.
                for (const auto& item2 : map) {
                    const FrequencyChannel* channel2 = item2.first;
                    const std::vector<unsigned int>& offsets2 = item2.second;
                    // Different channel...
                    if (*channel1 != *channel2) {
                        // ... then no slot of the first channel should equal a slot of the second...
                        for (unsigned int slot1 : offsets1)
                            CPPUNIT_ASSERT(std::find(offsets2.begin(), offsets2.end(), slot1) == offsets2.end());
                    }
                }
            }
//            coutd.setVerbose(false);
        }

        void testClearPendingRxReservations() {
            std::map<const FrequencyChannel *, std::vector<unsigned int>> proposed_resources;
            auto* rx_table = link_manager->current_reservation_table;
            auto* rx_table_2 = reservation_manager->getReservationTable(reservation_manager->getFreqChannelByCenterFreq(center_frequency2));
            rx_table->receiver_reservation_tables.clear();
            rx_table_2->receiver_reservation_tables.clear();
            // Make some reservations.
            size_t num_to_clear = 0;
            for (size_t offset = 2; offset < 10; offset++) {
                rx_table->mark(offset, Reservation(communication_partner_id, Reservation::RX));
                rx_table_2->mark(offset, Reservation(communication_partner_id, Reservation::RX));
                // These will be cleared afterwards.
                if (offset > 5) {
                    proposed_resources[rx_table->getLinkedChannel()].push_back(offset);
                    num_to_clear++;
                } else {
                    proposed_resources[rx_table_2->getLinkedChannel()].push_back(offset);
                    num_to_clear++;
                }
            }
            // Make sure setting worked.
            for (size_t offset = 0; offset < planning_horizon; offset++) {
                if (offset >= 2 && offset < 10)
                    CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, rx_table->getReservation(offset).getAction());
                else
                    CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, rx_table->getReservation(offset).getAction());
            }
            // Clear proposed <channel, slots>.
            CPPUNIT_ASSERT_EQUAL(num_to_clear, lme->clearPendingRxReservations(proposed_resources, 0, 0));
            // Ensure that the proposed resources were cleared, all others weren't.
            for (size_t offset = 0; offset < planning_horizon; offset++) {
                if (offset >= 2 && offset < 10) {
                    if (offset <= 5)
                        CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, rx_table->getReservation(offset).getAction());
                    else
                        CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, rx_table->getReservation(offset).getAction());
                    if (offset > 5)
                        CPPUNIT_ASSERT_EQUAL(Reservation::Action::RX, rx_table_2->getReservation(offset).getAction());
                    else
                        CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, rx_table_2->getReservation(offset).getAction());
                } else {
                    CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, rx_table->getReservation(offset).getAction());
                    CPPUNIT_ASSERT_EQUAL(Reservation::Action::IDLE, rx_table_2->getReservation(offset).getAction());
                }
            }
        }

        CPPUNIT_TEST_SUITE(LinkManagementEntityTests);
            CPPUNIT_TEST(testSchedule);
            CPPUNIT_TEST(testUpdate);
            CPPUNIT_TEST(testPopulateRequest);
            CPPUNIT_TEST(testClearPendingRxReservations);
        CPPUNIT_TEST_SUITE_END();
    };
}