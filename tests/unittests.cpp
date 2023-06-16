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

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/TestResultCollector.h>
#include "../coutdebug.hpp"
#include "MovingAverageTests.cpp"
#include "ReservationTests.cpp"
#include "ReservationTableTests.cpp"
#include "ReservationManagerTests.cpp"
#include "FrequencyChannelTests.cpp"
#include "SHLinkManagerTests.cpp"
#include "MCSOTDMA_MacTests.cpp"
#include "SystemTests.cpp"
#include "MCSOTDMA_PhyTests.cpp"
#include "ManyUsersTests.cpp"
#include "PPLinkManagerTests.cpp"
#include "ThirdPartyLinkTests.cpp"
#include "LinkProposalFinderTests.cpp"
#include "SlotCalculatorTests.cpp"

int main() {	
	CppUnit::TextUi::TestRunner runner;

	runner.addTest(MovingAverageTests::suite());
	runner.addTest(ReservationTests::suite());
	runner.addTest(ReservationTableTests::suite());
	runner.addTest(ReservationManagerTests::suite());
	runner.addTest(FrequencyChannelTests::suite());	
	runner.addTest(MCSOTDMA_MacTests::suite());
	runner.addTest(MCSOTDMA_PhyTests::suite());	
	runner.addTest(SHLinkManagerTests::suite());
	runner.addTest(SystemTests::suite());
	runner.addTest(ManyUsersTests::suite());
	runner.addTest(PPLinkManagerTests::suite());
	runner.addTest(ThirdPartyLinkTests::suite());
	runner.addTest(LinkProposalFinderTests::suite());	
	runner.addTest(SlotCalculatorTests::suite());	

	runner.run();
	return runner.result().wasSuccessful() ? 0 : 1;
}