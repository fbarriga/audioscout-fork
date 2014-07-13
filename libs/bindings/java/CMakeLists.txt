cmake_minimum_required(VERSION 2.6)
find_package(JNI)

include_directories(${JNI_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR})

add_library(pHashAudio-jni SHARED org_phash_AudioHash.cpp)

target_link_libraries(pHashAudio-jni AudioData pHashAudio)

install(TARGETS pHashAudio-jni DESTINATION lib)