add_definitions(-g -O0 -Wall -DDEBUG)

FILE(GLOB CPPSources *.cpp)
FILE(GLOB CPPHeaders *.hpp)

add_executable(cattenacio_dpi ${CPPSources})
add_library(cdpi SHARED ${CPPSources})

set(LIBS event boost_thread-mt boost_system-mt boost_regex-mt pcap)

target_link_libraries(cattenacio_dpi ${LIBS})
target_link_libraries(cdpi ${LIBS})

INSTALL(TARGETS cdpi LIBRARY DESTINATION lib)
INSTALL(FILES ${CPPHeaders} DESTINATION include/cdpi)
