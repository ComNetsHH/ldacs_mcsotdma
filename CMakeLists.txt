cmake_minimum_required(VERSION 3.13)
project(tuhh_intairnet_mc-sotdma)

set(CMAKE_CXX_STANDARD 14)

add_library(tuhh_intairnet_mc-sotdma SHARED L2Packet.hpp ReservationTable.cpp ReservationTable.hpp)

add_executable(unittests tests/unittests.cpp tests/ReservationTableTests.cpp)
target_link_libraries(unittests :libcppunit.so tuhh_intairnet_mc-sotdma)