

cmake_minimum_required(VERSION 2.8.9)

project(video-streamer)

set(deploy_include_path "/shared/include")

################################
#  toolchain, dependency, etc.
################################

SET( CMAKE_CXX_COMPILER "/usr/bin/g++-5"  )
SET( CMAKE_C_COMPILER "gcc"  )
#SET( CMAKE_CXX_COMPILER "icc" )

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=auto")

SET(CMAKE_CXX_FLAGS_DEBUG
		"${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -g -O0 -Wall  -Wfatal-errors")
SET(CMAKE_CXX_FLAGS_RELEASE "-std=c++11 -g -Wall -O3 -DHAVE_NUMA")

SET(CMAKE_C_FLAGS_DEBUG
		"${CMAKE_C_FLAGS_DEBUG} -std=gnu11 -g -O0 -Wall -Wfatal-errors")

set(deploy_target_path "localhost:/tmp/$ENV{USER}")

################################
#  headers, source
################################

# add required libraries
# all libraries are in /shared/hikey
include_directories(
        ./
        /shared/ffmpeg/include
        /shared/libzmq/include
)

link_directories(
        /shared/ffmpeg/lib
        /shared/libzmq/lib
)


file (GLOB HEADERS
        *.h
		include/*.h
		include/mm/*.h
		include/xplane/*.h
)

SET(SOURCE
	measure.c
	mm.cpp
    ${HEADERS}
    )

SET (FFMPEG_LIBS
        avdevice
        avfilter
        avformat
        avcodec
        swresample
        swscale
        avutil
#				X11 va-x11 va-drm  va # intel's video accel lib etc
#        vdpau  # nvidia decoding & presentation
)

SET (COMMON_LIBS
        m
        xcb
        xcb-shm
        xcb-shape
        dl
        bz2
        z
        pthread
        lzma
        boost_program_options
        boost_serialization
        X11
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=30)
    add_definitions(-DDEBUG)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=50)
	add_definitions(-DNDEBUG)
endif ()
####################################################################################


#SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/library")
#SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/library")
# for store static library to diff directory
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")

#SET(CMAKE_C_FLAGS	"${CMAKE_C_FLAGS} -finline-functions")

#SET(CMAKE_EXE_LINKER_FLAGS	)

#FIND_LIBRARY(test NAMES libtest.a PATHS .)

#add_custom_command(TARGET xplane POST_BUILD
#		COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/lib/libxplane.a ${deploy_target_path}/lib/libxplane-${CMAKE_BUILD_TYPE}.a
#		COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/include/xplane_lib.h ${deploy_include_path}
#		COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/include/xplane/* ${deploy_include_path}/xplane/
#		)

########################################
# Test
########################################

########################################
add_executable(hw_decode.bin
		examples/hw_decode.c
		)
TARGET_LINK_LIBRARIES(hw_decode.bin
        ${FFMPEG_LIBS}
        ${COMMON_LIBS}
		)
########################################
add_executable(demuxing_decoding.bin
        examples/demuxing_decoding.c
        )
TARGET_LINK_LIBRARIES(demuxing_decoding.bin
        ${FFMPEG_LIBS}
        m
        xcb
        xcb-shm
        xcb-shape
        dl
        bz2
        z
        pthread
        lzma
        )
########################################
add_executable(decode_video.bin
        examples/decode_video.c
        )
TARGET_LINK_LIBRARIES(decode_video.bin
        ${FFMPEG_LIBS}
        m
        xcb
        xcb-shm
        xcb-shape
        dl
        bz2
        z
        pthread
        lzma
        )
########################################
add_executable(taskvent.bin
        examples/taskvent.cpp
        )
TARGET_LINK_LIBRARIES(taskvent.bin
        zmq
        )
########################################
add_executable(taskwork2.bin
        examples/taskwork2.cpp
        )
TARGET_LINK_LIBRARIES(taskwork2.bin
        zmq
        )
########################################
SET(PROGRAM test-frame-puller)
add_executable(${PROGRAM}.bin
        test/${PROGRAM}.cpp
        )
TARGET_LINK_LIBRARIES(${PROGRAM}.bin
        ${COMMON_LIBS}
        zmq
        )
add_custom_command(TARGET ${PROGRAM}.bin POST_BUILD
        COMMAND scp ${PROGRAM}.bin ${deploy_target_path}/${CMAKE_BUILD_TYPE}/
        )
########################################
SET(BINARY chunk-pusher.bin)
add_executable(${BINARY}
        mm.cpp
        rxtx.cpp
        measure.c
        test/test-chunk-pusher.cpp
        )
TARGET_LINK_LIBRARIES(${BINARY}
        ${FFMPEG_LIBS}
        ${COMMON_LIBS}
        zmq
        )
add_custom_command(TARGET ${BINARY} POST_BUILD
        COMMAND scp ${BINARY} ${deploy_target_path}/${CMAKE_BUILD_TYPE}/
        )
########################################
SET(BINARY chunk-puller.bin)
add_executable(${BINARY}
        mm.cpp
        measure.c
        rxtx.cpp
        test/test-chunk-puller.cpp
        )
TARGET_LINK_LIBRARIES(${BINARY}
        ${FFMPEG_LIBS}
        ${COMMON_LIBS}
        zmq
        )
add_custom_command(TARGET ${BINARY} POST_BUILD
        COMMAND scp ${BINARY} ${deploy_target_path}/${CMAKE_BUILD_TYPE}/
        )
########################################
SET(PROGRAM serv)
add_executable(${PROGRAM}.bin
        ${HEADERS}
        mydecoder_hw.cpp
#        mydecoder_sw.cpp       # obsoleted
        rxtx.cpp
        mm.cpp
        measure.c
        main_serv.cpp
        )
TARGET_LINK_LIBRARIES(${PROGRAM}.bin
        ${FFMPEG_LIBS}
        ${COMMON_LIBS}
        zmq
        )
add_custom_command(TARGET ${PROGRAM}.bin POST_BUILD
        COMMAND scp ${PROGRAM}.bin ${deploy_target_path}/${CMAKE_BUILD_TYPE}/
        )
########################################
SET(PROGRAM test-fb-sender)
add_executable(${PROGRAM}.bin
        test/${PROGRAM}.cpp
        rxtx.cpp
        mm.cpp
        measure.c
        )
TARGET_LINK_LIBRARIES(${PROGRAM}.bin
        ${FFMPEG_LIBS}
        ${COMMON_LIBS}
        zmq
        )
add_custom_command(TARGET ${PROGRAM}.bin POST_BUILD
        COMMAND scp ${PROGRAM}.bin ${deploy_target_path}/${CMAKE_BUILD_TYPE}/
        )
########################################
SET(PROGRAM test-fb-recver)
add_executable(${PROGRAM}.bin
        test/${PROGRAM}.cpp
        rxtx.cpp
        mm.cpp
        measure.c
        )
TARGET_LINK_LIBRARIES(${PROGRAM}.bin
        ${FFMPEG_LIBS}
        ${COMMON_LIBS}
        zmq
        )
add_custom_command(TARGET ${PROGRAM}.bin POST_BUILD
        COMMAND scp ${PROGRAM}.bin ${deploy_target_path}/${CMAKE_BUILD_TYPE}/
        )
########################################
SET(PROGRAM test-decoder)
add_executable(${PROGRAM}.bin
        test/${PROGRAM}.cpp
        rxtx.cpp
        mm.cpp
        measure.c
        mydecoder_hw.cpp
        )
TARGET_LINK_LIBRARIES(${PROGRAM}.bin
        ${FFMPEG_LIBS}
        ${COMMON_LIBS}
        zmq
        )
add_custom_command(TARGET ${PROGRAM}.bin POST_BUILD
        COMMAND scp ${PROGRAM}.bin ${deploy_target_path}/${CMAKE_BUILD_TYPE}/
        )
########################################
