cmake_minimum_required(VERSION 3.13)
project(tuhh_intairnet_mc-sotdma)

set(CMAKE_CXX_STANDARD 14)
add_subdirectory(glue-lib-headers) # Gives access to the librar's CMakeLists.txt's variables.
include_directories(glue-lib-headers) # Includes the library header files, i.e. -I <dir>

# MC-SOTDMA source files.
set(MCSOTDMA_SRC ReservationTable.cpp ReservationTable.hpp ReservationManager.cpp ReservationManager.hpp FrequencyChannel.cpp FrequencyChannel.hpp Timestamp.hpp Reservation.cpp Reservation.hpp CPRPosition.hpp LinkManager.cpp LinkManager.hpp coutdebug.cpp coutdebug.hpp MCSOTDMA_Mac.cpp MCSOTDMA_Mac.hpp)
# MC-SOTDMA unittest files.
set(MCSOTDMA_TEST_SRC tests/unittests.cpp tests/ReservationTableTests.cpp tests/ReservationManagerTests.cpp tests/FrequencyChannelTests.cpp tests/ReservationTests.cpp)

# MC-SOTDMA library target.
add_library(tuhh_intairnet_mc-sotdma SHARED ${MCSOTDMA_SRC} ${GLUE_SRC_HPP})
# Link MC-SOTDMA library against shared Glue Library.
find_library(GLUE_LIBRARY intairnet_linklayer_glue HINTS glue-lib) # glue-lib is a symbolic link to the directory that contains the intairnet_linklayer_glue library.
target_link_libraries(tuhh_intairnet_mc-sotdma PUBLIC ${GLUE_LIBRARY})

# Unittest target.
add_executable(mcsotdma-unittests ${MCSOTDMA_SRC} ${MCSOTDMA_TEST_SRC})
target_link_libraries(mcsotdma-unittests :libcppunit.so tuhh_intairnet_mc-sotdma) # Link in MC-SOTDMA library.