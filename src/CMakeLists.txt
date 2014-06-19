cmake_policy(SET CMP0022 OLD)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -fPIC -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -fPIC -std=c++11")

option(USE_MONGO "Use MongoDB" 0)
option(USE_DIVERT "Use Divert" 0)
option(COMPILE_STATIC_LIB "Compile Static Library" 0)

FILE(GLOB CPPSources *.cpp)
FILE(GLOB CPPHeaders *.hpp)
FILE(GLOB CPPMain cdpi_main.cpp)

find_package(Boost COMPONENTS thread system regex iostreams filesystem REQUIRED)

IF(USE_DIVERT)
  add_definitions(-DUSE_DIVERT)
ENDIF(USE_DIVERT)

add_library(cdpi SHARED ${CPPSources})
add_executable(cattenacio_dpi ${CPPMain})

set(LIBS cdpi event event_pthreads ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_IOSTREAMS_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} pcap crypto pthread ssl)

target_link_libraries(cattenacio_dpi ${LIBS})
target_link_libraries(cdpi ${LIBS})

INSTALL(TARGETS cdpi cattenacio_dpi RUNTIME DESTINATION bin
                                    LIBRARY DESTINATION lib
                                    ARCHIVE DESTINATION lib)

IF(COMPILE_STATIC_LIB)
  add_library(cdpi_static STATIC ${CPPSources})
  INSTALL(TARGETS cdpi_static ARCHIVE DESTINATION lib)
ENDIF(COMPILE_STATIC_LIB)

INSTALL(FILES ${CPPHeaders} DESTINATION include/cdpi)
