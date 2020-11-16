//
// Created by Sebastian Lindner on 06.10.20.
//

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/BriefTestProgressListener.h>
#include "ReservationTableTests.cpp"
#include "FrequencyChannelTests.cpp"
#include "ReservationManagerTests.cpp"
#include "ReservationTests.cpp"
#include "QueueManagerTests.cpp"
#include "../coutdebug.hpp"

int main() {
	coutd.setVerbose(false);
	CppUnit::TestResult result;
	CppUnit::TestResultCollector collectedResults;
	CppUnit::BriefTestProgressListener progress;
	CppUnit::TextUi::TestRunner runner;
	result.addListener(&collectedResults);
	result.addListener(&progress);
	
	runner.addTest(ReservationTests::suite());
	runner.addTest(ReservationTableTests::suite());
	runner.addTest(FrequencyChannelTests::suite());
	runner.addTest(ReservationManagerTests::suite());
	runner.addTest(QueueManagerTests::suite());
	
	runner.run(result);
	return collectedResults.wasSuccessful() ? 0 : 1;
}