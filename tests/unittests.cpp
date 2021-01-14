//
// Created by Sebastian Lindner on 06.10.20.
//

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/TestResultCollector.h>
#include "MovingAverageTests.cpp"
#include "ReservationTableTests.cpp"
#include "FrequencyChannelTests.cpp"
#include "ReservationManagerTests.cpp"
#include "ReservationTests.cpp"
#include "../coutdebug.hpp"
#include "LinkManagerTests.cpp"
#include "MCSOTDMA_MacTests.cpp"
#include "BCLinkManagerTests.cpp"
#include "ContentionEstimatorTests.cpp"
#include "SystemTests.cpp"
#include "LinkRenewalProcessTests.cpp"

int main() {
	coutd.setVerbose(false);
	CppUnit::TextUi::TestRunner runner;
	
	runner.addTest(MovingAverageTests::suite());
	runner.addTest(ReservationTests::suite());
	runner.addTest(ReservationTableTests::suite());
	runner.addTest(FrequencyChannelTests::suite());
	runner.addTest(ReservationManagerTests::suite());
	runner.addTest(LinkManagerTests::suite());
	runner.addTest(BCLinkManagerTests::suite());
	runner.addTest(MCSOTDMA_MacTests::suite());
	runner.addTest(ContentionEstimatorTests::suite());
	runner.addTest(SystemTests::suite());
	runner.addTest(LinkRenewalProcessTests::suite());
	
	runner.run();
	return runner.result().wasSuccessful() ? 0 : 1;
}