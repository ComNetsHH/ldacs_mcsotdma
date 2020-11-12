//
// Created by Sebastian Lindner on 06.10.20.
//

#include <cppunit/ui/text/TestRunner.h>
#include "ReservationTableTests.cpp"
#include "FrequencyChannelTests.cpp"
#include "ReservationManagerTests.cpp"
#include "ReservationTests.cpp"
#include "L2HeaderTests.cpp"
#include "L2PacketTests.cpp"
#include "QueueManagerTests.cpp"
#include "../coutdebug.hpp"

int main() {
	coutd.setVerbose(false);
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(ReservationTests::suite());
	runner.addTest(ReservationTableTests::suite());
	runner.addTest(FrequencyChannelTests::suite());
	runner.addTest(ReservationManagerTests::suite());
	runner.addTest(L2HeaderTests::suite());
	runner.addTest(L2PacketTests::suite());
	runner.addTest(QueueManagerTests::suite());
	runner.run();
}