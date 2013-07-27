set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall")

option(USE_MONGO "Use MongoDB" 0)
option(USE_DIVERT "Use Divert" 0)

FILE(GLOB CPPSources *.cpp)
FILE(GLOB CPPHeaders *.hpp)
FILE(GLOB CPPMain cdpi_main.cpp)
FILE(GLOB CPPMongo cdpi_mongo.cpp)
FILE(GLOB CPPMongoSOA cdpi_mongo_soa.cpp)
FILE(GLOB CPPMongoCHAOS cdpi_mongo_chaos.cpp)

IF(USE_DIVERT)
  add_definitions(-DUSE_DIVERT)
ENDIF(USE_DIVERT)

list(REMOVE_ITEM CPPSources ${CPPMain} ${CPPMongo} ${CPPMongoSOA} ${CPPMongoCHAOS})

add_executable(cattenacio_dpi ${CPPSources} ${CPPMain})
add_library(cdpi SHARED ${CPPSources})
add_library(cdpi_static STATIC ${CPPSources})

IF(APPLE)
    add_library(cdpi_static_i386 STATIC ${CPPSources})
    set_property(TARGET cdpi_static_i386 PROPERTY COMPILE_FLAGS "-arch i386")
ENDIF(APPLE)

set(LIBS event boost_thread-mt boost_system-mt boost_regex-mt boost_iostreams-mt pcap crypto)
set(LIBS_MONGO mongoclient ssl boost_filesystem-mt)

target_link_libraries(cattenacio_dpi ${LIBS})
target_link_libraries(cdpi ${LIBS})

INSTALL(TARGETS cdpi cdpi_static cattenacio_dpi RUNTIME DESTINATION bin
                                                LIBRARY DESTINATION lib
                                                ARCHIVE DESTINATION lib)
INSTALL(FILES ${CPPHeaders} DESTINATION include/cdpi)

IF(APPLE)
    INSTALL(TARGETS cdpi_static_i386 ARCHIVE DESTINATION lib)
ENDIF(APPLE)

IF(USE_MONGO)
    add_executable(cdpi_mongo ${CPPSources} ${CPPMongo})
    add_executable(cdpi_mongo_soa ${CPPMongoSOA})
    add_executable(cdpi_mongo_chaos ${CPPMongoCHAOS})
    target_link_libraries(cdpi_mongo ${LIBS_MONGO} ${LIBS})
    target_link_libraries(cdpi_mongo_soa ${LIBS_MONGO} ${LIBS} unbound cdpi)
    target_link_libraries(cdpi_mongo_chaos ${LIBS_MONGO} ${LIBS} unbound  event cdpi)
    INSTALL(TARGETS cdpi_mongo cdpi_mongo_soa RUNTIME DESTINATION bin)
ENDIF(USE_MONGO)
