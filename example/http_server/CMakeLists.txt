# 加载子目录
# set(HTTP_SRCS
#   HttpServer.cc
#   HttpResponse.cc
#   HttpContext.cc
#   main.cc
# )

aux_source_directory(${PROJECT_SOURCE_DIR}/example/http_server HTTP_SRCS)

add_executable(httpserver ${HTTP_SRCS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/example/http_server)

target_link_libraries(httpserver muduo-http)

