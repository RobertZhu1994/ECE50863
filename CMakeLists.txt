

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

set(deploy_target_path "/shared/avx2")

################################
#  headers, source
################################

# add required libraries
# all libraries are in /shared/hikey
include_directories(
        ./
        /shared/ffmpeg/include
)

link_directories(
        /shared/ffmpeg/lib
)


file (GLOB HEADERS
		include/*.h
		include/mm/*.h
		include/xplane/*.h
)

SET(SOURCE
	mm/batch.c
	mm/mempool.cpp
	${SOURCE_XPLANE}
	${SOURCE_SORT_MERGE}

	xplane_lib.cpp
	measure.c
    ${HEADERS}
    )

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DCONFIG_KAGE_GLOBAL_DEBUG_LEVEL=50)
	#	add_definitions(-D_GLIBCXX_DEBUG)	   # will fail compilation in folly
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
		avdevice
		avfilter
		avformat
		avcodec
		swresample
		swscale
		avutil
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

