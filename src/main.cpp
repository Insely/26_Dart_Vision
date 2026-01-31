#include "camera/camera.hpp"
#include "detector/detector.hpp"
#include "serial/serial.hpp"
#include <iostream>

#define DEBUG_MODE

int main() {
  // 串口初始化
  SerialPort serial("/dev/ttyUSB0", 115200);
  serial.init();

  // 改用工业相机
  Camera camera;
  if (!camera.init("../configs/settings.yaml")) {
    std::cerr << "Camera Init Failed!" << std::endl;
    return -1;
  }

  Detector detector; // 实例化检测器
  detector.init("../configs/detector.yaml");
  // 创建展示窗口
  cv::namedWindow("demo", cv::WINDOW_NORMAL);

#ifdef DEBUG_MODE
  cv::namedWindow("Grayscale", cv::WINDOW_NORMAL); // 灰度窗口
  cv::namedWindow("Binarized", cv::WINDOW_NORMAL); // 二值化窗口
#endif
  // 2. 调整窗口到指定尺寸（宽度, 高度）
  // 注意：这会改变窗口大小，但图像可能会按比例缩放或拉伸
  cv::resizeWindow("demo", 800, 600);

#ifdef DEBUG_MODE
  cv::resizeWindow("Grayscale", 800, 600);
  cv::resizeWindow("Binarized", 800, 600);
#endif

  while (true) {
    cv::Mat frame;
    if (!camera.getFrame(frame)) {
      // 如果获取失败，可能是连接断开，尝试重连或退出
      // 这里简单处理为跳过
      continue;
    }
    if (frame.empty())
      break;

    // --- 核心算法处理 ---
    DetectionResult result = detector.process(frame);

    //  通信
    if (result.is_locked) {
      VisionData packet;

      // 1. 计算水平偏差
      packet.yaw_error = (int)result.error_x;

      // 2. 目标是否到达中心 (设置 5 像素的死区)
      packet.at_center = (std::abs(packet.yaw_error) < 10.0f) ? 1 : 0;

      // 2. 合理的开火判定逻辑：
      // 只有当目标稳定锁定（is_locked为真）且物理对准中心（at_center为真）时，才允许发射
      if (packet.at_center == 1) {
        printf("允许发射\n");
        packet.allow_fire = 1; // 允许发射
      } else {
        packet.allow_fire = 0; // 即使锁定了，如果没对准中心，也不准发射
      }

      // 发送数据
      serial.send(packet);
      std::cout << "Data Sent: Yaw=" << packet.yaw_error << std::endl;
    } else {
      // 目标丢失，也发一个空包告知电控
      VisionData lost_packet;
      lost_packet.yaw_error = 0;
      lost_packet.at_center = 0;
      lost_packet.allow_fire = 0;
      serial.send(lost_packet);
    }

    // --- 绘图与显示逻辑 (保持在 main 方便观察) ---
    cv::Mat display_frame =
        frame.clone(); // 深拷贝，防止原图像被污染，影响下一轮的图像识别

    if (result.is_locked) {
      cv::circle(display_frame, result.center, 12, cv::Scalar(0, 0, 255), -1);
      std::string info = "LOCKED | X_Err: " +
                         std::to_string((int)result.error_x); // 拼接字符串
      cv::putText(display_frame, info, cv::Point(30, 50),
                  cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0),
                  2); // 打印字符到窗口上
    } else {
      cv::putText(display_frame, "SEARCHING...", cv::Point(30, 50),
                  cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 255), 2);
    }

    cv::imshow("demo", display_frame);

#ifdef DEBUG_MODE
    cv::imshow("Grayscale", detector.getGray());
    cv::imshow("Binarized", detector.getMask());
#endif

    // --- 必须添加以下代码！！！ ---
    // 这里的 30 代表每帧等待 30 毫秒，大约对应 33 FPS
    int key = cv::waitKey(1); // 30
    if (key == 27)            // 按下 ESC 键退出
      break;
    if (key == 's') {
      // 保存逻辑...
    }
  }

  return 0;
}