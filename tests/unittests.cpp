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
#include "LinkManagerTests.cpp"
#include "BCLinkManagerTests.cpp"
#include "MCSOTDMA_MacTests.cpp"
#include "ContentionEstimatorTests.cpp"
#include "SystemTests.cpp"
#include "LinkManagementProcessTests.cpp"

int main() {
	coutd.setVerbose(false);
	CppUnit::TextUi::TestRunner runner;
	
//	runner.addTest(MovingAverageTests::suite());
//	runner.addTest(ReservationTests::suite());
//	runner.addTest(ReservationTableTests::suite());
//    runner.addTest(ReservationManagerTests::suite());
//	runner.addTest(FrequencyChannelTests::suite());
	runner.addTest(LinkManagerTests::suite());
//	runner.addTest(BCLinkManagerTests::suite());
//	runner.addTest(MCSOTDMA_MacTests::suite());
//	runner.addTest(ContentionEstimatorTests::suite());
//	runner.addTest(SystemTests::suite());
//	runner.addTest(LinkManagementProcessTests::suite());
	
	runner.run();
	return runner.result().wasSuccessful() ? 0 : 1;
}