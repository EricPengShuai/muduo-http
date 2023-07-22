# 指定使用 CMake 的最低版本号
cmake_minimum_required(VERSION 3.0)

# 设置项目名称
project(muduo-http C CXX)

# 添加搜索路径
include_directories(
    ${PROJECT_SOURCE_DIR}/include/base
    ${PROJECT_SOURCE_DIR}/include/net
    ${PROJECT_SOURCE_DIR}/include/timer
    # ${PROJECT_SOURCE_DIR}/src/memory
    # ${PROJECT_SOURCE_DIR}/src/mysql
    )

# 包含这些目录下的.cc文件
aux_source_directory(${PROJECT_SOURCE_DIR}/src/base SRC_BASE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/net SRC_NET)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/timer SRC_TIMER)
# aux_source_directory(${PROJECT_SOURCE_DIR}/src/memory SRC_MEMORY)
# aux_source_directory(${PROJECT_SOURCE_DIR}/src/mysql SRC_MYSQL)

# 设置编译选项
set(CXX_FLAGS
    -g
    -Wall
    -std=c++11
    )

# 生成动态库 muduo-http
add_library(muduo-http SHARED 
            ${SRC_BASE}
            ${SRC_NET}
            ${SRC_TIMER}
            # ${SRC_MEMORY}
            # ${SRC_MYSQL}
            )

# 目标动态库所需连接的库（这里需要连接libpthread.so）
target_link_libraries(muduo-http pthread mysqlclient)

# 设置生成动态库的路径，放在根目录的lib文件夹下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置安装的默认路径
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})

# 将目录安装到 include/muduo-http 下（只安装头文件）
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/ DESTINATION include/muduo-http
        FILES_MATCHING PATTERN "*.h")

# 加载 example
add_subdirectory(example/echo_server)

add_subdirectory(example/timer_server)

add_subdirectory(example/http_server)
