cmake_minimum_required(VERSION 3.10)
project(DartDemo)

# 1. 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 寻找 OpenCV 库
find_package(OpenCV REQUIRED)

# 3. 收集所有源文件 (递归搜索 src 目录下所有的 .cpp)
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 4. 生成可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 5. 指定头文件包含路径 (核心修改点)
target_include_directories(${PROJECT_NAME} PRIVATE 
    include                # 这样可以用 #include "detector/detector.hpp"
    ${OpenCV_INCLUDE_DIRS} # 显式加入 OpenCV 的头文件路径
)

# 6. 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})