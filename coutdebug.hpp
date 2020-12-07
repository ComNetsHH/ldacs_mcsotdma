//
// Created by Sebastian Lindner on 11.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_COUTD_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_COUTD_HPP


#include <iostream>

class coutdebug {
	private:
		bool verbose;
		unsigned int num_indents = 0;
	
	public:
		explicit coutdebug(bool verbose);
		
		void setVerbose(bool verbose);
		
		bool isVerbose();
		
		void increaseIndent() {
			num_indents++;
			std::cout << "\t";
		}
		
		void decreaseIndent() {
			num_indents--;
			flush();
		}
		
		void setIndent(unsigned int num_indents) {
			this->num_indents = num_indents;
		}
		
		template<class T> coutdebug& operator<<(const T &x) {
			if (verbose) {
				std::cout << x;
			}
			return *this;
		}
		
		void flush();
		
		// This is the type of std::cout.
		typedef std::basic_ostream<char, std::char_traits<char>> CoutType;
		
		// std::endl is a function with this signature.
		typedef CoutType& (*StandardEndLine)(CoutType&);
		
		// Define what to do when std::endl is passed to coutd.
		coutdebug& operator<<(StandardEndLine endl) {
			if (verbose) {
				endl(std::cout);
				for (unsigned int i = 0; i < num_indents; i++)
					std::cout << "\t";
			}
			return *this;
		}
};

extern coutdebug coutd;



#endif //TUHH_INTAIRNET_MC_SOTDMA_COUTD_HPP
