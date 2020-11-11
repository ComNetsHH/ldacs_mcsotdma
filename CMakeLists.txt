cmake_minimum_required(VERSION 3.13)
project(tuhh_intairnet_mc-sotdma)

set(CMAKE_CXX_STANDARD 14)

add_library(tuhh_intairnet_mc-sotdma SHARED L2Packet.hpp ReservationTable.cpp ReservationTable.hpp ReservationManager.cpp ReservationManager.hpp FrequencyChannel.cpp FrequencyChannel.hpp Timestamp.hpp Reservation.cpp Reservation.hpp IcaoId.hpp L2Header.hpp QueueManager.cpp QueueManager.hpp CPRPosition.hpp LinkManager.cpp LinkManager.hpp)

add_executable(unittests tests/unittests.cpp tests/ReservationTableTests.cpp L2Packet.hpp ReservationTable.cpp ReservationTable.hpp ReservationManager.cpp ReservationManager.hpp tests/ReservationManagerTests.cpp FrequencyChannel.cpp FrequencyChannel.hpp tests/FrequencyChannelTests.cpp Timestamp.hpp Reservation.cpp Reservation.hpp IcaoId.hpp tests/ReservationTests.cpp L2Header.hpp tests/L2HeaderTests.cpp QueueManager.cpp QueueManager.hpp tests/L2PacketTests.cpp tests/QueueManagerTests.cpp CPRPosition.hpp LinkManager.cpp LinkManager.hpp)
target_link_libraries(unittests :libcppunit.so tuhh_intairnet_mc-sotdma)