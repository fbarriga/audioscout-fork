cmake_minimum_required(VERSION 2.6)
project(CSharpBindings)

set(CSharpBindings_VERSION_MAJOR 1)
set(CSharpBindings_VERSION_MINOR 0)

# build a dll for the csharp bindings to pHashAudio native c library

set(PHASHAUDIO_NI_SRCS audiodata.cs phashaudio.cs)
set(TESTNI_SRCS TestNI.cs)

set(NI_DLL_SRCS audiodata.cs phashaudio.cs)
set(NI_TEST_SRCS TestNI.cs)
set(NI_DLL_NAME pHashAudio-ni.dll)
set(NI_TEST_NAME TestNI.exe)

add_custom_command( OUTPUT ${NI_DLL_NAME} COMMAND csc ARGS /unsafe /platform:x86 /target:library /out:${NI_DLL_NAME} ${NI_DLL_SRCS})

add_custom_command( OUTPUT ${NI_TEST_NAME} COMMAND csc ARGS /unsafe /platform:x86 /target:exe /out:${NI_TEST_NAME} /reference:${NI_DLL_NAME} ${NI_TEST_SRCS})

add_custom_target(ni_library SOURCES ${NI_DLL_NAME})

add_custom_target(ni_test ALL DEPENDS ni_library SOURCES ${NI_TEST_NAME})


