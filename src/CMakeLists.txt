set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall")

FILE(GLOB CPPSources *.cpp)
FILE(GLOB CPPHeaders *.hpp)
FILE(GLOB CPPMain cdpi_main.cpp)

list(REMOVE_ITEM CPPSources ${CPPMain})

add_executable(cattenacio_dpi ${CPPSources} ${CPPMain})
add_library(cdpi SHARED ${CPPSources})
add_library(cdpi_static STATIC ${CPPSources})

IF(APPLE)
    add_library(cdpi_static_i386 STATIC ${CPPSources})
    set_property(TARGET cdpi_static_i386 PROPERTY COMPILE_FLAGS "-arch i386")
ENDIF(APPLE)

set(LIBS event boost_thread-mt boost_system-mt boost_regex-mt pcap crypto)

target_link_libraries(cattenacio_dpi ${LIBS})
target_link_libraries(cdpi ${LIBS})


INSTALL(TARGETS cdpi cdpi_static LIBRARY DESTINATION lib
                                 ARCHIVE DESTINATION lib)
INSTALL(FILES ${CPPHeaders} DESTINATION include/cdpi)

IF(APPLE)
    INSTALL(TARGETS cdpi_static_i386 ARCHIVE DESTINATION lib)
ENDIF(APPLE)
