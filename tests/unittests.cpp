//
// Created by Sebastian Lindner on 06.10.20.
//

#include <cppunit/ui/text/TestRunner.h>
#include "ReservationTableTests.cpp"
#include "FrequencyChannelTests.cpp"

int main() {
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(ReservationTableTests::suite());
	runner.addTest(FrequencyChannelTests::suite());
	runner.run();
}