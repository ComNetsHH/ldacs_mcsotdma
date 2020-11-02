cmake_minimum_required(VERSION 3.13)
project(tuhh_intairnet_mc-sotdma)

set(CMAKE_CXX_STANDARD 14)

add_library(tuhh_intairnet_mc-sotdma SHARED L2Packet.hpp ReservationTable.cpp ReservationTable.hpp ReservationManager.cpp ReservationManager.hpp FrequencyChannel.cpp FrequencyChannel.hpp)

add_executable(unittests tests/unittests.cpp tests/ReservationTableTests.cpp L2Packet.hpp ReservationTable.cpp ReservationTable.hpp ReservationManager.cpp ReservationManager.hpp tests/ReservationManagerTests.cpp FrequencyChannel.cpp FrequencyChannel.hpp tests/FrequencyChannelTests.cpp)
target_link_libraries(unittests :libcppunit.so tuhh_intairnet_mc-sotdma)