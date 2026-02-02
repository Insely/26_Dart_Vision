#pragma once
#include <opencv2/opencv.hpp>

// 定义检测结果结构体，方便 main 函数直接使用
struct DetectionResult {
  bool is_locked;     // 是否锁定
  cv::Point2f center; // 目标的中心点坐标
  float error_x;      // X 轴偏差
};

class Detector {
public:
  Detector();

  // 初始化探测器，加载配置文件
  bool init(const std::string &config_path);
  /**
   * @brief 核心处理函数
   * @param frame 输入的原始彩色图
   * @return DetectionResult 包含状态和偏差的结果
   */
  // 注意：这里参数带了 const，表示函数内部不会修改原图
  DetectionResult process(const cv::Mat &frame);
  // 获取中间过程图（用于 main 函数中的 DEBUG 显示）
  cv::Mat getMask() const { return mask_; }
  cv::Mat getGray() const { return gray_; }

private:
  // 1. 内部处理步骤封装
  void preprocess(const cv::Mat &input);
  bool findTarget(const cv::Mat &input, cv::Point2f &best_center);

  // 2. 算法参数
  // HSV 阈值
  int h_min_ = 35;
  int h_max_ = 90;
  int s_min_ = 43;
  int s_max_ = 255;
  int v_min_ = 46;
  int v_max_ = 255;

  double min_area_ = 200.0; // 最小面积过滤
  int min_found_frame_ = 3; // 连续检测帧数阈值

  // 3. 内部状态变量
  int found_count_ = 0;       // 连续检测到的帧数状态
  cv::Mat hsv_, gray_, mask_; // 中间处理图
};