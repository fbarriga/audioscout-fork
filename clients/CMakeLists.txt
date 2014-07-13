cmake_minimum_required(VERSION 2.6)
project(AudioScoutClient)

set(AudioScoutClient_MAJOR_VERSION 1)
set(AudioScoutClient_MINOR_VERSION 0)

add_definitions("-g -O3")

include_directories("${PROJECT_SOURCE_DIR}/../libs")
link_directories("${PROJECT_BINARY_DIR}/../libs")

add_executable(auscoutclient auscout-client.c)

target_link_libraries(auscoutclient AudioData pHashAudio zmq)

set(AudioScoutClient_HEADERS 
     mainwindow.h
     SendThread.h
     MeterWidget.h
)

set(AudioScoutClient_SOURCES
     auscout-client-main.cpp
      mainwindow.cpp 
      SendThread.cpp
      MeterWidget.cpp
)

configure_file(auscout-gui "${PROJECT_BINARY_DIR}/auscout-gui" COPY_ONLY)


set(PERMS OWNER_READ OWNER_WRITE OWNER_EXECUTE 
          GROUP_READ GROUP_EXECUTE 
          WORLD_READ WORLD_EXECUTE)

install(TARGETS auscoutclient DESTINATION bin)

