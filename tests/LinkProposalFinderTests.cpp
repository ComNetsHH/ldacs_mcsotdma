#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../LinkProposalFinder.hpp"
#include "MockLayers.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class LinkProposalFinderTests : public CppUnit::TestFixture {
	private:
		TestEnvironment* env;
		MacId partner_id = MacId(42);
		MacId own_id = MacId(41);
		ReservationManager *reservation_manager;

	public:
		void setUp() override {
			env = new TestEnvironment(own_id, partner_id);
			reservation_manager = env->mac_layer->getReservationManager();
		}

		void tearDown() override {
			delete env;
		}

		void testFind() {
			size_t num_proposals = 3;
			int min_offset = 1;
			std::vector<LinkProposal> proposals = LinkProposalFinder::findLinkProposals(num_proposals, min_offset, 2, 2, 20, false, reservation_manager, env->mac_layer);
			CPPUNIT_ASSERT_EQUAL(num_proposals, proposals.size());						
			for (size_t i = 0; i < proposals.size() - 1; i++) 
				CPPUNIT_ASSERT_LESS(proposals.at(i+1).center_frequency, proposals.at(i).center_frequency);
			for (size_t i = 0; i < proposals.size(); i++) 
				CPPUNIT_ASSERT_EQUAL(min_offset, proposals.at(i).slot_offset);
				
		}		

	CPPUNIT_TEST_SUITE(LinkProposalFinderTests);
			CPPUNIT_TEST(testFind);			
		CPPUNIT_TEST_SUITE_END();
	};

}