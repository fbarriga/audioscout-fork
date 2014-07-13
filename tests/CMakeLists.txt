cmake_minimum_required(VERSION 2.6)
project(tests)

add_definitions(-g -Wall -O0)

include_directories("${PROJECT_SOURCE_DIR}/../libs")
link_directories("${PROJECT_BINARY_DIR}/../libs")

add_executable(auscoutdriver auscout-driver.c)
target_link_libraries(auscoutdriver zmq m pthread AudioData pHashAudio rt)

add_executable(auscoutsink auscout-sink.c)
target_link_libraries(auscoutsink zmq pthread rt)

add_executable(tbldriver tblserver_driver.c)
target_link_libraries(tbldriver zmq m pthread AudioData pHashAudio rt)

add_executable(mdriver metadatadb_driver.c)
target_link_libraries(mdriver zmq pthread AudioData pHashAudio rt)

add_executable(RetrieveClient retrieve_client.c)
target_link_libraries(RetrieveClient zmq)

add_executable(TestAudioData test_audiodata.c)
target_link_libraries(TestAudioData AudioData zmq)

add_executable(TestAudioHash testaudiohash.c)
target_link_libraries(TestAudioHash AudioData pHashAudio zmq)

add_executable(TestIndex test_index.c)
target_link_libraries(TestIndex pHashAudio m)

add_executable(testmerge testmergeindex.c)
target_link_libraries(testmerge pHashAudio m)

add_executable(testserialize testserialize.c)
