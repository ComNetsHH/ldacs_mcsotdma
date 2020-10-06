//
// Created by Sebastian Lindner on 06.10.20.
//

#include <cppunit/ui/text/TestRunner.h>
#include "ReservationTableTests.cpp"

int main() {
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(ReservationTableTests::suite());
	runner.run();
}