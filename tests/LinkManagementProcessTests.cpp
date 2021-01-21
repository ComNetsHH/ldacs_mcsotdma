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
    class LinkManagementProcessTests : public CppUnit::TestFixture {
    private:
        LinkManager* link_manager;
        ReservationManager* reservation_manager;
        MacId own_id = MacId(42);
        MacId communication_partner_id = MacId(43);
        uint32_t planning_horizon = 128;
        uint64_t center_frequency1 = 962, center_frequency2 = 963, center_frequency3 = 964, bc_frequency = 965, bandwidth = 500;
        unsigned long num_bits_going_out = 800*100;
        MACLayer* mac;
        ARQLayer* arq_layer;
        RLCLayer* rlc_layer;
        PHYLayer* phy_layer;
        NetworkLayer* net_layer;

        unsigned int tx_timeout = 5, init_offset = 1, tx_offset = 3, num_renewal_attempts = 2;
        LinkManagementEntity* lme;

    public:
        void setUp() override {
            phy_layer = new PHYLayer(planning_horizon);
            mac = new MACLayer(own_id, planning_horizon);
            reservation_manager = mac->reservation_manager;
            reservation_manager->setTransmitterReservationTable(phy_layer->getTransmitterReservationTable());
            reservation_manager->addFrequencyChannel(false, bc_frequency, bandwidth);
            reservation_manager->addFrequencyChannel(true, center_frequency1, bandwidth);
            reservation_manager->addFrequencyChannel(true, center_frequency2, bandwidth);
            reservation_manager->addFrequencyChannel(true, center_frequency3, bandwidth);

            link_manager = new LinkManager(communication_partner_id, reservation_manager, mac);
            arq_layer = new ARQLayer();
            mac->setUpperLayer(arq_layer);
            arq_layer->setLowerLayer(mac);
            net_layer = new NetworkLayer();
            rlc_layer = new RLCLayer(own_id);
            net_layer->setLowerLayer(rlc_layer);
            rlc_layer->setUpperLayer(net_layer);
            rlc_layer->setLowerLayer(arq_layer);
            arq_layer->setUpperLayer(rlc_layer);
            phy_layer->setUpperLayer(mac);
            mac->setLowerLayer(phy_layer);

            lme = new LinkManagementEntity(link_manager);
        }

        void tearDown() override {
            delete mac;
            delete link_manager;
            delete arq_layer;
            delete rlc_layer;
            delete phy_layer;
            delete net_layer;
            delete lme;
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
            coutd.setVerbose(true);
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
            coutd.setVerbose(false);
        }

        CPPUNIT_TEST_SUITE(LinkManagementProcessTests);
            CPPUNIT_TEST(testSchedule);
            CPPUNIT_TEST(testUpdate);
            CPPUNIT_TEST(testPopulateRequest);
        CPPUNIT_TEST_SUITE_END();
    };
}