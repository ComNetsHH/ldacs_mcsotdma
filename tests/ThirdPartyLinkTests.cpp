//
// Created by seba on 4/14/21.
//

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../ThirdPartyLink.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class ThirdPartyLinkTests : public CppUnit::TestFixture {
	private:
		ThirdPartyLink *link;
		MacId id1, id2;

	public:
		void setUp() override {
			id1 = MacId(42);
			id2 = MacId(43);
			link = new ThirdPartyLink(id1, id2);
		}

		void tearDown() override {
			delete link;
		}

		void testOne() {

		}		

		CPPUNIT_TEST_SUITE(ThirdPartyLinkTests);
			CPPUNIT_TEST(testOne);			
		CPPUNIT_TEST_SUITE_END();
	};

}