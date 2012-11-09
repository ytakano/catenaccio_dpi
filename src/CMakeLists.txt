add_definitions(-g -O0 -Wall -DDEBUG)

add_executable(cattenacio_dpi cdpi_main.cpp cdpi_divert.cpp cdpi_id.cpp cdpi_tcp.cpp cdpi_stream.cpp cdpi_http.cpp cdpi_proto.cpp)
target_link_libraries(cattenacio_dpi event boost_thread-mt boost_system-mt boost_regex-mt)
