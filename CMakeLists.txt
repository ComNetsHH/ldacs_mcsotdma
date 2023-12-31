# The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
# Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.1)
project(tuhh_intairnet_mc-sotdma)

set(CMAKE_CXX_STANDARD 14)
add_subdirectory(glue-lib-headers) # Gives access to the library's CMakeLists.txt's variables.

# MC-SOTDMA source files.
set(MCSOTDMA_SRC ReservationTable.cpp ReservationTable.hpp ReservationManager.cpp ReservationManager.hpp FrequencyChannel.cpp FrequencyChannel.hpp Reservation.cpp Reservation.hpp CPRPosition.hpp coutdebug.hpp MCSOTDMA_Mac.cpp MCSOTDMA_Mac.hpp BeaconPayload.hpp MCSOTDMA_Phy.cpp MCSOTDMA_Phy.hpp MovingAverage.cpp MovingAverage.hpp LinkManager.hpp LinkManager.cpp SHLinkManager.cpp SHLinkManager.hpp PPLinkManager.cpp PPLinkManager.hpp NeighborObserver.hpp NeighborObserver.cpp ReservationMap.hpp SlotCalculator.hpp SlotCalculator.cpp DutyCycle.hpp DutyCycle.cpp LinkProposalFinder.hpp LinkProposalFinder.cpp ThirdPartyLink.hpp ThirdPartyLink.cpp glue-lib-headers/Statistic.hpp glue-lib-headers/Statistic.cpp glue-lib-headers/MacId.hpp glue-lib-headers/LinkProposal.hpp)
# MC-SOTDMA unittest files.
set(MCSOTDMA_TEST_SRC tests/unittests.cpp tests/ReservationTableTests.cpp tests/ReservationManagerTests.cpp tests/FrequencyChannelTests.cpp tests/ReservationTests.cpp tests/MCSOTDMA_MacTests.cpp tests/MockLayers.hpp tests/SHLinkManagerTests.cpp tests/MovingAverageTests.cpp tests/MCSOTDMA_PhyTests.cpp tests/LinkProposalFinderTests.cpp tests/PPLinkManagerTests.cpp tests/SlotCalculatorTests.cpp tests/SystemTests.cpp tests/ThirdPartyLinkTests.cpp tests/ManyUsersTests.cpp ) 

# MC-SOTDMA library target.
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(tuhh_intairnet_mc-sotdma SHARED ${MCSOTDMA_SRC} ${GLUE_SRC_HPP})  # GLUE_SRC_HPP is from glue-lib-headers/CMakeLists.txt
# Link MC-SOTDMA library against shared Glue Library.
target_include_directories(tuhh_intairnet_mc-sotdma PUBLIC glue-lib-headers)
target_link_libraries(tuhh_intairnet_mc-sotdma LINK_PUBLIC intairnet_linklayer_glue)

# Unittest target.
add_executable(mcsotdma-unittests ${MCSOTDMA_SRC} ${MCSOTDMA_TEST_SRC} ${GLUE_SRC_HPP})
find_library(CPPUNITLIB cppunit)
if(CPPUNITLIB)
	target_include_directories(mcsotdma-unittests PUBLIC glue-lib-headers /opt/homebrew/opt/cppunit/include)
	target_link_libraries(mcsotdma-unittests PRIVATE ${CPPUNITLIB} tuhh_intairnet_mc-sotdma) # Link in cppunit and MC-SOTDMA libraries.
	target_compile_options(mcsotdma-unittests PRIVATE) #-Wall -Wextra -Wpedantic
endif()