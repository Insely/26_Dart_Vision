#include "camera/camera.hpp"
#include "detector/detector.hpp"
#include "serial/serial.hpp"
#include <iostream>

int main() {
  // --- 读取全局配置 ---
  cv::FileStorage settings("../configs/settings.yaml", cv::FileStorage::READ);
  if (!settings.isOpened()) {
    std::cerr << "Settings file not found!" << std::endl;
    return -1;
  }

  std::string serial_port = "/dev/ttyUSB0";
  int baudrate = 115200;
  int show_binarized = 1;
  int show_ui = 1;
  int use_demo = 0;
  std::string demo_path = "../demo.mp4";
  float yaw_offset = 0.0f;

  if (!settings["Settings"]["SerialPort"].empty())
    settings["Settings"]["SerialPort"] >> serial_port;
  if (!settings["Settings"]["Baudrate"].empty())
    settings["Settings"]["Baudrate"] >> baudrate;
  if (!settings["Settings"]["ShowBinarized"].empty())
    settings["Settings"]["ShowBinarized"] >> show_binarized;
  if (!settings["Settings"]["ShowUI"].empty())
    settings["Settings"]["ShowUI"] >> show_ui;
  if (!settings["Settings"]["UseDemo"].empty())
    settings["Settings"]["UseDemo"] >> use_demo;
  if (!settings["Settings"]["DemoPath"].empty())
    settings["Settings"]["DemoPath"] >> demo_path;
  if (!settings["Settings"]["YawOffset"].empty())
    settings["Settings"]["YawOffset"] >> yaw_offset;

  settings.release();

  // 串口初始化
  SerialPort serial(serial_port, baudrate);
  serial.init();

  // 相机 / Demo 初始化
  Camera camera;
  cv::VideoCapture demo_cap;

  if (use_demo) {
    demo_cap.open(demo_path);
    if (!demo_cap.isOpened()) {
      std::cerr << "Demo video not found: " << demo_path << std::endl;
      return -1;
    }
    std::cout << "[Main] Using demo video: " << demo_path << std::endl;
  } else {
    if (!camera.init("../configs/camera.yaml")) {
      std::cerr << "Camera Init Failed!" << std::endl;
      return -1;
    }
  }

  Detector detector;
  detector.init("../configs/detector.yaml");

  // 创建展示窗口
  if (show_ui) {
    cv::namedWindow("demo", cv::WINDOW_NORMAL);
    cv::resizeWindow("demo", 800, 600);
  }
  if (show_binarized) {
    cv::namedWindow("Binarized", cv::WINDOW_NORMAL);
    cv::resizeWindow("Binarized", 800, 600);
  }

  while (true) {
    cv::Mat frame;
    if (use_demo) {
      demo_cap >> frame;
      if (frame.empty()) {
        demo_cap.set(cv::CAP_PROP_POS_FRAMES, 0); // 循环播放
        continue;
      }
    } else {
      if (!camera.getFrame(frame)) {
        continue;
      }
      if (frame.empty())
        break;
    }

    // --- 核心算法处理 ---
    DetectionResult result = detector.process(frame);

    //  通信
    if (result.is_locked) {
      VisionData packet;

      // 1. 计算水平偏差（减去偏置，使物理对齐时 error 为 0）
      packet.yaw_error = (int)(result.error_x - yaw_offset);

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
                         std::to_string((int)(result.error_x - yaw_offset));
      cv::putText(display_frame, info, cv::Point(30, 50),
                  cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0),
                  2); // 打印字符到窗口上
    } else {
      cv::putText(display_frame, "SEARCHING...", cv::Point(30, 50),
                  cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 255), 2);
    }

    if (show_ui) {
      cv::imshow("demo", display_frame);
    }

    if (show_binarized) {
      cv::imshow("Binarized", detector.getMask());
    }

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