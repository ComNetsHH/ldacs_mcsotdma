#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../PPLinkManager.hpp"
#include "../SHLinkManager.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
class PPLinkManagerTests : public CppUnit::TestFixture {
private:
	PPLinkManager *pp;
	SHLinkManager *sh;
	MacId id, partner_id;
	uint32_t planning_horizon;
	MACLayer *mac, *mac_you;
	TestEnvironment *env, *env_you;

public:
	void setUp() override {
		id = MacId(42);
		partner_id = MacId(43);
		env = new TestEnvironment(id, partner_id);
		env_you = new TestEnvironment(partner_id, id);
		planning_horizon = env->planning_horizon;
		mac = env->mac_layer;		
		mac_you = env_you->mac_layer;
		pp = (PPLinkManager*) mac->getLinkManager(partner_id);
		sh = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);

		env->phy_layer->connected_phys.push_back(env_you->phy_layer);
		env_you->phy_layer->connected_phys.push_back(env->phy_layer);		
	}

	void tearDown() override {
		delete env;
	}

	void testGet() {
		CPPUNIT_ASSERT(pp != nullptr);
	}

	void testAskSHToSendLinkRequest() {		
		CPPUNIT_ASSERT_EQUAL(false, sh->isNextBroadcastScheduled());
		CPPUNIT_ASSERT_EQUAL(true, sh->link_requests.empty());
		pp->notifyOutgoing(100);
		CPPUNIT_ASSERT_EQUAL(true, sh->isNextBroadcastScheduled());
		CPPUNIT_ASSERT_EQUAL(false, sh->link_requests.empty());
		CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
		CPPUNIT_ASSERT_EQUAL(partner_id, sh->link_requests.at(0));
	}

	/** Tests that when there's no saved, advertised link, the SH initiates a two-way handshake. */
	void testSendLinkRequestWithNoAdvertisedLink() {
		mac->notifyOutgoing(1, partner_id);
		CPPUNIT_ASSERT_EQUAL(size_t(1), sh->link_requests.size());
		size_t request_tx_slot = sh->next_broadcast_slot;
		for (size_t t = 0; t < request_tx_slot; t++) {
			mac->update(1);
			mac->execute();
			mac->onSlotEnd();
		}
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_requests_sent.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_own_proposals_sent.get());		
	}

	/** Tests that when there is an advertised link, the SH initiates a 1SHOT establishment. */
	void testSendLinkRequestWithAdvertisedLink() {
		size_t num_slots = 0, max_slots = 50;
		while (mac->stat_num_broadcasts_rcvd.get() < 1.0 && num_slots++ < max_slots) {
			mac->update(1);
			mac_you->update(1);
			mac->execute();
			mac_you->execute();
			mac->onSlotEnd();
			mac_you->onSlotEnd();
		}
		CPPUNIT_ASSERT_LESS(max_slots, num_slots);
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac_you->stat_num_broadcasts_sent.get());
		CPPUNIT_ASSERT_EQUAL(size_t(1), (size_t) mac->stat_num_broadcasts_rcvd.get());
	}

	CPPUNIT_TEST_SUITE(PPLinkManagerTests);
		// CPPUNIT_TEST(testGet);		
		// CPPUNIT_TEST(testAskSHToSendLinkRequest);
		// CPPUNIT_TEST(testSendLinkRequestWithNoAdvertisedLink);				
		CPPUNIT_TEST(testSendLinkRequestWithAdvertisedLink);					
	CPPUNIT_TEST_SUITE_END();
};
}