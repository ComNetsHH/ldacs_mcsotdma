//
// Created by Sebastian Lindner on 11.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_COUTD_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_COUTD_HPP


#include <iostream>

class coutdebug {
	private:
		bool verbose;
	
	public:
		explicit coutdebug(bool verbose);
		
		void setVerbose(bool verbose);
		
		bool isVerbose();
		
		template<class T> coutdebug& operator <<(const T &x) {
			if (verbose)
				std::cout << x;
			return *this;
		}
		
		void flush();
		
		// This is the type of std::cout.
		typedef std::basic_ostream<char, std::char_traits<char>> CoutType;
		
		// std::endl is a function with this signature.
		typedef CoutType& (*StandardEndLine)(CoutType&);
		
		// Define what to do when std::endl is passed to coutd.
		coutdebug& operator<<(StandardEndLine endl) {
			if (verbose)
				endl(std::cout);
			return *this;
		}
};

extern coutdebug coutd;



#endif //TUHH_INTAIRNET_MC_SOTDMA_COUTD_HPP
