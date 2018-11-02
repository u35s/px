cmake_minimum_required(VERSION 2.6)

project(px)

add_definitions(-std=c++11)

include_directories(src)

aux_source_directory(src DIR_SRCS)

aux_source_directory(src/server DIR_SERVER_SRCS)

aux_source_directory(src/xlib DIR_XLIB_SRCS)

MESSAGE(STATUS "src:" +${DIR_SRCS})
MESSAGE(STATUS "src/server:" +${DIR_SERVER_SRCS})
MESSAGE(STATUS "src/xlib:" +${DIR_XLIB_SRCS})

add_executable( px ${DIR_SRCS} ${DIR_SERVER_SRCS} ${DIR_XLIB_SRCS})
