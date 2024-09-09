project(wifi_emu)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(INCLUDES_WIFIEMU inc src/utils)
include_directories(${INCLUDES_WIFIEMU})

file(GLOB_RECURSE SOURCES
    "src/utils/*.c"
    "src/*.cpp"
    "src/tests/*.cpp"
    "src/messages/*.cpp"
    "src/utils/*.cpp"
    "src/utils/*.hpp"
    "src/sta/*.cpp"
)

add_executable(cci ${SOURCES})
target_link_libraries(cci 
       pcap rdk_wifiemulatorhal wifi_webconfig stdc++fs cjson pthread rbus curl)

install(TARGETS cci DESTINATION bin)

set(SOURCES_BINARY2
    src/cci_cli/cci_cli.c
    )

add_executable(cci_cli ${SOURCES_BINARY2})
install(TARGETS cci_cli DESTINATION bin)
