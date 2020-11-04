//
// Created by Sebastian Lindner on 06.10.20.
//

#include <cppunit/ui/text/TestRunner.h>
#include "ReservationTableTests.cpp"
#include "FrequencyChannelTests.cpp"
#include "ReservationManagerTests.cpp"
#include "ReservationTests.cpp"

int main() {
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(ReservationTableTests::suite());
	runner.addTest(FrequencyChannelTests::suite());
	runner.addTest(ReservationManagerTests::suite());
	runner.addTest(ReservationTests::suite());
	runner.run();
}