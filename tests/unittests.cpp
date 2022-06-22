//
// Created by Sebastian Lindner on 06.10.20.
//

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
// #include "SystemTests.cpp"
#include "MCSOTDMA_PhyTests.cpp"
// #include "ThreeUsersTests.cpp"
#include "PPLinkManagerTests.cpp"
// #include "ThirdPartyLinkTests.cpp"
#include "LinkProposalFinderTests.cpp"
#include "SlotCalculatorTests.cpp"

int main() {	
	CppUnit::TextUi::TestRunner runner;

	// runner.addTest(MovingAverageTests::suite());
	// runner.addTest(ReservationTests::suite());
	// runner.addTest(ReservationTableTests::suite());
	// runner.addTest(ReservationManagerTests::suite());
	// runner.addTest(FrequencyChannelTests::suite());	
	// runner.addTest(MCSOTDMA_MacTests::suite());
	// runner.addTest(MCSOTDMA_PhyTests::suite());	
	// runner.addTest(SHLinkManagerTests::suite());
//	// runner.addTest(SystemTests::suite());
//	// runner.addTest(ThreeUsersTests::suite());
	// runner.addTest(PPLinkManagerTests::suite());
//	// runner.addTest(ThirdPartyLinkTests::suite());
	// runner.addTest(LinkProposalFinderTests::suite());	
	runner.addTest(SlotCalculatorTests::suite());	

	runner.run();
	return runner.result().wasSuccessful() ? 0 : 1;
}