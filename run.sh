#!/bin/bash

# 确保脚本在发生错误时立即停止（比如编译失败就不继续往下执行）
set -e

# 定义是否需要重新编译的变量，默认不重新编译 (false)
REBUILD=false
# 是否启用 imshow UI 显示
SHOW_UI=false

# 解析参数
while getopts "rs" opt; do
  case $opt in
    r)
      REBUILD=true
      ;;
    s)
      SHOW_UI=true
      ;;
    \?)
      echo "无效的参数! 使用方法: $0 [-r] [-s]"
      exit 1
      ;;
  esac
done

# 如果指定了 -r 参数，执行编译流程
if [ "$REBUILD" = true ]; then
  echo "======== [开始重新编译 DartVision] ========"
  
  # 创建 build 目录（如果不存在的话）
  mkdir -p build
  cd build
  
  # 执行 CMake 配置
  echo "--> 正在生成 Makefile..."
  cmake ..
  
  # 使用所有可用 CPU 核心进行编译
  # ${nproc:-2} 的意思是：如果获取不到 nproc，默认使用 2 核编译，防止报错
  NPROC=$(nproc 2>/dev/null || echo 2)
  echo "--> 正在编译 (使用 $NPROC 个核心)..."
  make -j"$NPROC"
  
  # 编译完成后回到项目根目录
  cd ..
  echo "======== [编译完成] ========"
fi

# 检查可执行文件是否存在
if [ ! -f "./build/DartVision" ]; then
  echo "错误: 未找到可执行文件 ./build/DartVision"
  echo "请先运行脚本并加上 -r 参数进行编译: $0 -r"
  exit 1
fi

# 启动程序
echo "======== [正在启动 DartVision] ========"
if [ "$SHOW_UI" = true ]; then
  sudo ./build/DartVision -s
else
  sudo ./build/DartVision
fi