cmake_minimum_required(VERSION 2.6)
project(AudioScout)
set(AudioScout_MAJOR_VERSION 1)
set(AudioScout_MINOR_VERSION 0)

add_subdirectory(libs)
add_subdirectory(servers)
add_subdirectory(clients)
add_subdirectory(tests)

