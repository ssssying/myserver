cmake_minimum_required(VERSION 3.6)
project(gameserver)
add_definitions(-D_DEBUG_ -DUSE_LOG4CPP)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(SOURCE_FILES
        main.cpp
        inc/gameserver.h
        src/gameserver.cpp
        logicmodule/inc/coremodule.h
        logicmodule/src/coremodule.cpp
        )

include_directories(
        /usr/local/include
        ${CMAKE_SOURCE_DIR/common/message/inc}
        ${CMAKE_SOURCE_DIR/common/timer}
        ${CMAKE_SOURCE_DIR/common/tools/inc}
)
link_directories(
        /usr/local/lib/
        ${CMAKE_SOURCE_DIR}/lib
)

add_executable(gameserver ${SOURCE_FILES})
target_link_libraries(gameserver common pthread protobuf )
set(EXECUTABgit LE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/)