cmake_minimum_required(VERSION 2.6)
project(pHashAudio)

set(pHashAudio_VERSION_MAJOR 1)
set(pHashAudio_VERSION_MINOR 0)

if (CMAKE_HOST_WIN32)
#  set windows variables here:
   message("win32 system detected")
   add_definitions(-g -Wall -O3)
   set(BUILD_DLL 1)
   set(HAVE_MPG123 1)
   set(HAVE_AMR 1)
   set(MPG123_LIB "mpg123")
endif(CMAKE_HOST_WIN32)

if (CMAKE_HOST_UNIX)
# set unix system variables
   message("unix system detected")
   add_definitions(-g -Wall -O3 -shared -fPIC)
   #find_library(HAVE_MPG123 mpg123)
   #find_library(HAVE_AMR opencore-amrnb)
   #if (HAVE_MPG123)
     #set(MPG123_LIB "mpg123")
   #endif(HAVE_MPG123)
   #if (HAVE_AMR)
     #set(AMR_LIB "opencore-amrnb")
   #endif(HAVE_AMR)
endif(CMAKE_HOST_UNIX)

add_subdirectory(table-4.3.0phmodified)

configure_file(
  "${PROJECT_SOURCE_DIR}/pHashAudioConfig.h.in"
  "${PROJECT_BINARY_DIR}/pHashAudioConfig.h"
)

include_directories("${PROJECT_BINARY_DIR}")
link_directories("${PROJECT_BINARY_DIR}/table-4.3.0phmodified")

add_library(pHashAudio SHARED phash_audio.c fft.c phcomplex.c)
target_link_libraries(pHashAudio table)

add_library(AudioData SHARED audiodata.c)
target_link_libraries(AudioData  sndfile ${MPG123_LIB} ${AMR_LIB} samplerate zmq)

if (CMAKE_HOST_UNIX)
   add_executable(audioindex audio_index.c)
   target_link_libraries(audioindex AudioData pHashAudio m)
endif(CMAKE_HOST_UNIX)

if (CMAKE_HOST_WIN32)
# compile csharp bindings for windows
#  add_subdirectory(bindings/csharp)
endif (CMAKE_HOST_WIN32)

#install files 

if(CMAKE_HOST_WIN32)
#set to your mingw installation or lib files are to be installed
   set(CMAKE_INSTALL_PREFIX "/c/users/david/mingw")
   install(FILES libpHashAudio.dll libAudioData.dll DESTINATION bin)
   install(FILES libpHashAudio.dll.a libAudioData.dll.a DESTINATION lib)
else(CMAKE_HOST_WIN32)
   install(TARGETS pHashAudio AudioData DESTINATION lib)
endif(CMAKE_HOST_WIN32)

install(FILES phash_audio.h DESTINATION include)
install(FILES audiodata.h DESTINATION include)
install(FILES pHashAudioConfig.h DESTINATION include)

if (CMAKE_HOST_UNIX)
   install(TARGETS audioindex DESTINATION bin)
endif(CMAKE_HOST_UNIX)
