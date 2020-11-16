cmake_minimum_required(VERSION 3.13)
project(tuhh_intairnet_mc-sotdma)

set(CMAKE_CXX_STANDARD 14)
include_directories(glue-lib-headers) # glue-lib-headers is a symbolic link to the Glue Library header files.

set(TARGET_SRC ReservationTable.cpp ReservationTable.hpp ReservationManager.cpp ReservationManager.hpp FrequencyChannel.cpp FrequencyChannel.hpp Timestamp.hpp Reservation.cpp Reservation.hpp QueueManager.cpp QueueManager.hpp CPRPosition.hpp LinkManager.cpp LinkManager.hpp L2Layer.cpp L2Layer.hpp coutdebug.cpp coutdebug.hpp)
set(TARGET_INCL glue-lib-headers/CPRPosition.hpp glue-lib-headers/IArq.hpp glue-lib-headers/IMac.hpp glue-lib-headers/IRlc.hpp glue-lib-headers/L2Header.hpp glue-lib-headers/L2Packet.hpp glue-lib-headers/MacId.hpp glue-lib-headers/SequenceNumber.cpp glue-lib-headers/SequenceNumber.hpp)
set(TARGET_TESTS tests/unittests.cpp tests/ReservationTableTests.cpp tests/ReservationManagerTests.cpp tests/FrequencyChannelTests.cpp tests/ReservationTests.cpp tests/QueueManagerTests.cpp)

add_library(tuhh_intairnet_mc-sotdma SHARED ${TARGET_SRC} ${TARGET_INCL})
find_library(GLUE_LIBRARY intairnet_linklayer_glue HINTS glue-lib) # glue-lib is a symbolic link to the directory that contains the intairnet_linklayer_glue library.
target_link_libraries(tuhh_intairnet_mc-sotdma PUBLIC ${GLUE_LIBRARY})

add_executable(unittests ${TARGET_SRC} ${TARGET_TESTS})
target_link_libraries(unittests :libcppunit.so tuhh_intairnet_mc-sotdma)