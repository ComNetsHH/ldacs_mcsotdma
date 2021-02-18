//
// Created by seba on 2/18/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../P2PLinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class P2PLinkManagerTests : public CppUnit::TestFixture {
	private:
		uint32_t planning_horizon = 1024;
		P2PLinkManager *link_manager;
		MacId id;

	public:
		void setUp() override {
			id = MacId(42);
			link_manager = new P2PLinkManager(id, nullptr, nullptr, 0, 0);
		}

		void tearDown() override {
			delete link_manager;
		}

	CPPUNIT_TEST_SUITE(P2PLinkManagerTests);
	CPPUNIT_TEST_SUITE_END();
	};
}