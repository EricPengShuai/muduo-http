add_executable(timerserver TimerServer.cpp)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/example/timer_server)

target_link_libraries(timerserver muduo-http)