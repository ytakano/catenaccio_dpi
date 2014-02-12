set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -fPIC")

option(MIND_YOUR_PRIVACY "Build MindYourPrivacy" 0)

IF(MIND_YOUR_PRIVACY)
  include_directories(../../src)

  FILE(GLOB CPPSources *.cpp)
  FILE(GLOB CPPHeaders *.hpp)
  FILE(GLOB CPPMain cdpi_mongo_soa.cpp)

  find_package(Boost COMPONENTS system filesystem REQUIRED)

  list(REMOVE_ITEM CPPSources ${CPPMain})

  add_executable(cdpi_mongo_soa ${CPPMain})

  set(LIBS unbound cdpi ${Boost_SYSTEM_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} crypto)

  target_link_libraries(cdpi_mongo_soa mongoclient ssl pthread ${LIBS})

  INSTALL(TARGETS cdpi_mongo_soa RUNTIME DESTINATION bin)
ENDIF(MIND_YOUR_PRIVACY)
