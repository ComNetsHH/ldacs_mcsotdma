//
// Created by Sebastian Lindner on 11.11.20.
//

#include <fstream>
#include "coutdebug.hpp"

coutdebug::coutdebug(bool verbose) : verbose(verbose) {
	// Empty constructor.
}

void coutdebug::setVerbose(bool verbose) {
	this->verbose = verbose;
}

bool coutdebug::isVerbose() {
	return this->verbose;
}

void coutdebug::flush() {
	if (verbose)
		dout.flush();
}

coutdebug coutd = coutdebug(true);

#if 0
std::ostream &dout = std::cout;
#else
std::ofstream dev_null("/dev/null");
std::ostream &dout = dev_null;
#endif