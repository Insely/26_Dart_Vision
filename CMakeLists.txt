cmake_minimum_required(VERSION 3.10)
project(DartDemo)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)

# 1. 寻找 OpenCV 库
find_package(OpenCV REQUIRED)

# 告诉 CMake 源文件在哪里
set(SOURCES
    "Core/Src/main.cpp"
    # 如果有其他 .cpp 文件也要加在这里
)

# 告诉 CMake 头文件在哪里
include_directories(Core/Inc)

# 生成可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 3. 链接 OpenCV 库
target_link_libraries(DartDemo PRIVATE ${OpenCV_LIBS})