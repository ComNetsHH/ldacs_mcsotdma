// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
			int num_bursts_forward = 1, num_bursts_reverse = 1, period = 1, timeout = 3;
			std::vector<LinkProposal> proposals = LinkProposalFinder::findLinkProposals(num_proposals, min_offset, num_bursts_forward, num_bursts_reverse, period, timeout, false, env->mac_layer->getReservationManager(), env->mac_layer);
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