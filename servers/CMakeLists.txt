cmake_minimum_required(VERSION 2.6)

project(Servers)

set(Servers_MAJOR_VERSION 1)
set(Servers_MINOR_VERSION 0)


add_definitions("-g -O3 -shared -fPIC")

include_directories("${PROJECT_SOURCE_DIR}/../libs")
link_directories   ("${PROJECT_BINARY_DIR}/../libs")

find_library(HAVE_PTHREAD pthread)
if (!HAVE_PTHREAD)
message(FATAL_ERROR "No pthread library found.")
endif(!HAVE_PTHREAD)

add_executable(auscoutd auscout.c)
target_link_libraries(auscoutd AudioData zmq rt pthread)

add_executable(tblservd table_server.c)
target_link_libraries(tblservd pHashAudio zmq pthread)


find_library(HAVE_SQLITE3 sqlite3)
if (!HAVE_SQLITE3)
message(FATAL_ERROR "No sqlite3 library found.")
endif (!HAVE_SQLITE3)

add_executable(metadatadb metadatadb.c)
target_link_libraries(metadatadb sqlite3 zmq pthread)

install(TARGETS auscoutd   DESTINATION bin)
install(TARGETS tblservd   DESTINATION bin)
install(TARGETS metadatadb DESTINATION bin)
