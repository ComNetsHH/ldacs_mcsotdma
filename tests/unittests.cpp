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
#include "ContentionEstimatorTests.cpp"
#include "SystemTests.cpp"
#include "MCSOTDMA_PhyTests.cpp"
#include "BeaconModuleTests.cpp"
#include "CongestionEstimatorTests.cpp"
#include "ThreeUsersTests.cpp"
#include "NewPPLinkManagerTests.cpp"

int main() {
	coutd.setVerbose(true);
	CppUnit::TextUi::TestRunner runner;

	// runner.addTest(MovingAverageTests::suite());
	// runner.addTest(ReservationTests::suite());
	// runner.addTest(ReservationTableTests::suite());
	// runner.addTest(ReservationManagerTests::suite());
	// runner.addTest(FrequencyChannelTests::suite());	
	// runner.addTest(MCSOTDMA_MacTests::suite());
	// runner.addTest(MCSOTDMA_PhyTests::suite());
	// runner.addTest(BeaconModuleTests::suite());
	// runner.addTest(CongestionEstimatorTests::suite());
	// runner.addTest(ContentionEstimatorTests::suite());
	// runner.addTest(SHLinkManagerTests::suite());
	runner.addTest(SystemTests::suite());
	// runner.addTest(ThreeUsersTests::suite());
	// runner.addTest(NewPPLinkManagerTests::suite());

	runner.run();
	return runner.result().wasSuccessful() ? 0 : 1;
}