cmake_minimum_required(VERSION 3.1)
project(tuhh_intairnet_mc-sotdma)

set(CMAKE_CXX_STANDARD 14)
add_subdirectory(glue-lib-headers) # Gives access to the library's CMakeLists.txt's variables.

# MC-SOTDMA source files.
set(MCSOTDMA_SRC ReservationTable.cpp ReservationTable.hpp ReservationManager.cpp ReservationManager.hpp FrequencyChannel.cpp FrequencyChannel.hpp Reservation.cpp Reservation.hpp CPRPosition.hpp coutdebug.cpp coutdebug.hpp MCSOTDMA_Mac.cpp MCSOTDMA_Mac.hpp BeaconPayload.hpp MCSOTDMA_Phy.cpp MCSOTDMA_Phy.hpp MovingAverage.cpp MovingAverage.hpp ContentionEstimator.cpp ContentionEstimator.hpp LinkManager.hpp LinkManager.cpp SHLinkManager.cpp SHLinkManager.hpp BeaconModule.cpp BeaconModule.hpp CongestionEstimator.cpp CongestionEstimator.hpp LinkInfoPayload.hpp LinkInfo.cpp LinkInfo.hpp NeighborObserver.hpp NeighborObserver.cpp NewPPLinkManager.hpp NewPPLinkManager.cpp ThirdPartyLink.hpp ThirdPartyLink.cpp ReservationMap.hpp glue-lib-headers/Statistic.hpp glue-lib-headers/Statistic.cpp glue-lib-headers/MacId.hpp)
# MC-SOTDMA unittest files.
set(MCSOTDMA_TEST_SRC tests/unittests.cpp tests/ReservationTableTests.cpp tests/ReservationManagerTests.cpp tests/FrequencyChannelTests.cpp tests/ReservationTests.cpp tests/MCSOTDMA_MacTests.cpp tests/MockLayers.hpp tests/SHLinkManagerTests.cpp tests/MovingAverageTests.cpp tests/ContentionEstimatorTests.cpp tests/MCSOTDMA_PhyTests.cpp tests/SystemTests.cpp tests/BeaconModuleTests.cpp tests/CongestionEstimatorTests.cpp tests/ThreeUsersTests.cpp tests/NewPPLinkManagerTests.cpp tests/ThirdPartyLinkTests.cpp)

# MC-SOTDMA library target.
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(tuhh_intairnet_mc-sotdma SHARED ${MCSOTDMA_SRC} ${GLUE_SRC_HPP})  # GLUE_SRC_HPP is from glue-lib-headers/CMakeLists.txt
# Link MC-SOTDMA library against shared Glue Library.
target_include_directories(tuhh_intairnet_mc-sotdma PUBLIC glue-lib-headers)
target_link_libraries(tuhh_intairnet_mc-sotdma LINK_PUBLIC intairnet_linklayer_glue)

# Unittest target.
add_executable(mcsotdma-unittests ${MCSOTDMA_SRC} ${MCSOTDMA_TEST_SRC} ${GLUE_SRC_HPP})
target_include_directories(mcsotdma-unittests PUBLIC glue-lib-headers)
target_link_libraries(mcsotdma-unittests PRIVATE cppunit tuhh_intairnet_mc-sotdma) # Link in cppunit and MC-SOTDMA libraries.
