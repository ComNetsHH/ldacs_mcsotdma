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
	MACLayer *mac;
	TestEnvironment *env;

public:
	void setUp() override {
		id = MacId(42);
		partner_id = MacId(43);
		env = new TestEnvironment(id, partner_id);
		planning_horizon = env->planning_horizon;
		mac = env->mac_layer;
		pp = (PPLinkManager*) mac->getLinkManager(partner_id);
		sh = (SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST);
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

	

	CPPUNIT_TEST_SUITE(PPLinkManagerTests);
		CPPUNIT_TEST(testGet);		
		CPPUNIT_TEST(testAskSHToSendLinkRequest);
	CPPUNIT_TEST_SUITE_END();
};
}