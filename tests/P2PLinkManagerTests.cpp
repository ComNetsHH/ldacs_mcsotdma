//
// Created by seba on 2/18/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class P2PLinkManagerTests : public CppUnit::TestFixture {
	private:
		uint32_t planning_horizon = 1024;


	public:
		void setUp() override {

		}

		void tearDown() override {

		}

	CPPUNIT_TEST_SUITE(P2PLinkManagerTests);
	CPPUNIT_TEST_SUITE_END();
	};
}