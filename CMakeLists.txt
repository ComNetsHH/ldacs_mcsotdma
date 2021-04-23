cmake_minimum_required(VERSION 3.13)
project(tuhh_intairnet_mc-sotdma)

set(CMAKE_CXX_STANDARD 14)
add_subdirectory(glue-lib-headers) # Gives access to the library's CMakeLists.txt's variables.
include_directories(glue-lib-headers) # Includes the library header files, i.e. -I <dir>

# MC-SOTDMA source files.
set(MCSOTDMA_SRC ReservationTable.cpp ReservationTable.hpp ReservationManager.cpp ReservationManager.hpp FrequencyChannel.cpp FrequencyChannel.hpp Reservation.cpp Reservation.hpp CPRPosition.hpp coutdebug.cpp coutdebug.hpp MCSOTDMA_Mac.cpp MCSOTDMA_Mac.hpp BeaconPayload.hpp MCSOTDMA_Phy.cpp MCSOTDMA_Phy.hpp MovingAverage.cpp MovingAverage.hpp ContentionEstimator.cpp ContentionEstimator.hpp P2PLinkManager.cpp P2PLinkManager.hpp LinkManager.hpp LinkManager.cpp BCLinkManager.cpp BCLinkManager.hpp BeaconModule.cpp BeaconModule.hpp CongestionEstimator.cpp CongestionEstimator.hpp LinkInfoPayload.hpp LinkInfo.cpp LinkInfo.hpp)
# MC-SOTDMA unittest files.
set(MCSOTDMA_TEST_SRC tests/unittests.cpp tests/ReservationTableTests.cpp tests/ReservationManagerTests.cpp tests/FrequencyChannelTests.cpp tests/ReservationTests.cpp tests/MCSOTDMA_MacTests.cpp tests/MockLayers.hpp tests/BCLinkManagerTests.cpp tests/MovingAverageTests.cpp tests/ContentionEstimatorTests.cpp tests/MCSOTDMA_PhyTests.cpp tests/P2PLinkManagerTests.cpp tests/SystemTests.cpp tests/BeaconModuleTests.cpp tests/CongestionEstimatorTests.cpp tests/ThreeUsersTests.cpp)

# Remove if not needed
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/include -I/usr/local/include/ -L/usr/local/Cellar/cppunit/1.15.1/lib")

# MC-SOTDMA library target.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(tuhh_intairnet_mc-sotdma SHARED ${MCSOTDMA_SRC} ${GLUE_SRC_HPP})
# Link MC-SOTDMA library against shared Glue Library.
target_link_libraries(tuhh_intairnet_mc-sotdma LINK_PUBLIC intairnet_linklayer_glue)

# Unittest target.
add_executable(mcsotdma-unittests ${MCSOTDMA_SRC} ${MCSOTDMA_TEST_SRC} ${GLUE_SRC_HPP})
target_link_libraries(mcsotdma-unittests cppunit tuhh_intairnet_mc-sotdma) # Link in cppunit and MC-SOTDMA libraries.