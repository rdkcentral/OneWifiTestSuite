cmake_minimum_required(VERSION 3.10)
project(external_agent_cci)

# Recursively find all .cpp files in the subproject
file(GLOB_RECURSE SUBPROJECT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# Add library target for the subproject with discovered source files
add_executable(${EXECUTABLE_NAME} ${SUBPROJECT_SOURCES} ${MAIN_SRC})

# Set include directories
target_include_directories(${EXECUTABLE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/http_server ${CMAKE_CURRENT_SOURCE_DIR}/ext_inc)

target_link_libraries(${EXECUTABLE_NAME}
    pcap rdk_wifihal  wifi_webconfig stdc++fs cjson pthread rbus curl syscfg secure_wrapper)
