cmake_minimum_required(VERSION 3.16.0)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# ------------------------------------------------------------------------------
# Project name setting
# ------------------------------------------------------------------------------
set(PROJ_NAME movieplayer)
project(${PROJ_NAME})

option(BUILD_TEST "build test programs" OFF)
option(MOVIEPLAYER_EXTERNAL_SDLAUDIO "use external sdl audio" OFF)
option(MOVIEPLAYER_EXTERNAL_MINIAUDIO "use external miniaudio" OFF)

if(MOVIEPLAYER_EXTERNAL_SDLAUDIO)
set(AUDIO_ENGINE_SOURCES src/common/AudioEngineSDL.cpp)
else()
set(AUDIO_ENGINE_SOURCES src/common/AudioEngine.cpp)
endif()

# ------------------------------------------------------------------------------
# Win64 check / Must call after project()
#   ${CMAKE_GENERATOR_PLATFORM} must be passed with -A option when running cmake
#   since it doesn't seem to be set otherwise, determine by WIN32 check + pointer size check
# ------------------------------------------------------------------------------
if(WIN32 AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
set(WIN64 1)
endif()

# ------------------------------------------------------------------------------
# Common
# ------------------------------------------------------------------------------
# for libyuv
# Whether to consider TEST in if (TEST) as an operator. Since yuv uses it as an OPTION variable,
# temporarily fix to OLD behavior. The policy does not propagate to subdirectories, so changing the default.
# (Why not just edit yuv's CMakeLists.txt? Well, that's true.)
#   Win64 refers to prebuilt binaries because SSE/AVX code does not support VS build,
#   so yuv's CMakeLists.txt is not referenced.
if(NOT WIN64)

cmake_policy(GET CMP0064 pre_cmp0064)
set(CMAKE_POLICY_DEFAULT_CMP0064 OLD)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/extlibs/libyuv)
cmake_policy(SET CMP0064 ${pre_cmp0064})

endif()

# ------------------------------------------------------------------------------
# Android
# ------------------------------------------------------------------------------

if(ANDROID)

add_library(${PROJ_NAME} STATIC
	src/common/MessageLooper.cpp
	src/common/PixelConvert.cpp
	src/common/MediaClock.cpp
	${AUDIO_ENGINE_SOURCES}
	src/android/TrackPlayer.cpp
	src/android/VideoTrackPlayer.cpp
	src/android/AudioTrackPlayer.cpp
	src/android/MoviePlayerCore.cpp
	src/android/MoviePlayer.cpp
	)

target_compile_definitions(${PROJ_NAME} PUBLIC
	INNER_AUDIOENGINE=1
)

if(MOVIEPLAYER_EXTERNAL_SDLAUDIO)
find_package(SDL3 REQUIRED)
target_compile_definitions(${PROJ_NAME} PUBLIC
	EXTERNAL_SDL=1
)
target_link_libraries(${PROJ_NAME} PUBLIC SDL3::SDL3)
else()
#  miniaudio
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
target_include_directories(${PROJ_NAME} PRIVATE ${MINIAUDIO_INCLUDE_DIRS})
if(MOVIEPLAYER_EXTERNAL_MINIAUDIO)
target_compile_definitions(${PROJ_NAME} PUBLIC
	EXTERNAL_MINIAUDIO=1
)
endif()
endif()


target_include_directories(${PROJ_NAME} 
PUBLIC 
	include
PRIVATE
	src/android
	extlibs/libyuv/include
	src/common
)

find_library(log-lib log)

target_link_libraries(${PROJ_NAME} PUBLIC
	${log-lib}
	android
	mediandk
	OpenMAXAL
	yuv
	)

else()

# ------------------------------------------------------------------------------
# Common architecture supporting nestegg version
# ------------------------------------------------------------------------------

if(WIN32)
# * add_compile_options needs to be called before add_library()/add_executable(),
#   so even though it's "common", this specification is placed here.
# Ignore unicode warnings
add_compile_options(/wd4819 /wd4244 /wd4267)
endif()

add_library(${PROJ_NAME} STATIC
	src/common/MessageLooper.cpp
	src/common/PixelConvert.cpp
	${AUDIO_ENGINE_SOURCES}
	src/common/MediaClock.cpp
	src/windows/Decoder.cpp
	src/windows/VpxDecoder.cpp
	src/windows/VorbisDecoder.cpp
	src/windows/OpusDecoder.cpp
	src/windows/WebmExtractor.cpp
	src/windows/MkvFileReader.cpp
	src/windows/MkvStreamReader.cpp
	src/windows/MoviePlayerCore.cpp
	src/windows/MoviePlayer.cpp
	extlibs/nestegg/src/nestegg.c
)

target_compile_definitions(${PROJ_NAME} PUBLIC
	INNER_AUDIOENGINE=1
)

if(MOVIEPLAYER_EXTERNAL_SDLAUDIO)
find_package(SDL3 REQUIRED)
target_compile_definitions(${PROJ_NAME} PUBLIC
	EXTERNAL_SDL=1
)
target_link_libraries(${PROJ_NAME} PUBLIC SDL3::SDL3)
else()
#  miniaudio
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
target_include_directories(${PROJ_NAME} PRIVATE ${MINIAUDIO_INCLUDE_DIRS})
if(MOVIEPLAYER_EXTERNAL_MINIAUDIO)
target_compile_definitions(${PROJ_NAME} PUBLIC
	EXTERNAL_MINIAUDIO=1
)
endif()
endif()

target_include_directories(${PROJ_NAME} 
PUBLIC 
	include
PRIVATE
	src/windows
	extlibs/nestegg/include
	extlibs/libyuv/include
	src/common
	src/windows
	)

# vpx
find_package(unofficial-libvpx CONFIG REQUIRED)
target_link_libraries(${PROJ_NAME} PUBLIC unofficial::libvpx::libvpx)

# libyuv (refer to the one under extlibs/)
# Since 64-bit build of SSE/AVX capable code is not supported by VS,
# link binaries already created with clang
if(WIN64)
message("*** Target is WIN64. Use prebuild libyuv-x64 binary. ***")
find_library(LIBYUV_WIN64_DBG yuv_debug ${CMAKE_CURRENT_LIST_DIR}/extlibs/prebuild/win64)
find_library(LIBYUV_WIN64_REL yuv_release ${CMAKE_CURRENT_LIST_DIR}/extlibs/prebuild/win64)
target_link_libraries(${PROJ_NAME} PUBLIC
	debug ${LIBYUV_WIN64_DBG}
	optimized ${LIBYUV_WIN64_REL})
else()
target_link_libraries(${PROJ_NAME} PUBLIC yuv)
endif()

# ogg
find_package(Ogg CONFIG REQUIRED)
target_link_libraries(${PROJ_NAME} PUBLIC Ogg::ogg)

# vorbis
find_package(Vorbis CONFIG REQUIRED)
target_link_libraries(${PROJ_NAME} PUBLIC Vorbis::vorbis)

# opus
find_package(Opus CONFIG REQUIRED)
target_link_libraries(${PROJ_NAME} PUBLIC Opus::opus)

# ------------------------------------------------------------------------------
# nestegg版対応アーキテクチャ個別：Linux
# ------------------------------------------------------------------------------
if(LINUX)
# pthread のリンクエラーを解消する
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()

if (BUILD_TEST)
add_subdirectory(test/windows)
endif()

# ANDROID
endif()