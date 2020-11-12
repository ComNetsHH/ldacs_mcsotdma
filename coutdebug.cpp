//
// Created by Sebastian Lindner on 11.11.20.
//

#include "coutdebug.hpp"

coutdebug::coutdebug(bool verbose)  : verbose(verbose){
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
		std::cout.flush();
}

coutdebug coutd = coutdebug(true);