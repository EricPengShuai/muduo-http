add_executable(echoserver EchoServer.cpp)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/example/echo_server)

target_link_libraries(echoserver muduo-http)