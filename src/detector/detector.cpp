#include "detector/detector.hpp"

// 修复 1: 使用 found_count_ (匹配头文件)
Detector::Detector() : found_count_(0) {}

bool Detector::init(const std::string &config_path) {
  if (config_path.empty())
    return false;

  cv::FileStorage fs(config_path, cv::FileStorage::READ);
  if (!fs.isOpened()) {
    std::cerr << "Detector Config file not found: " << config_path << std::endl;
    return false;
  }

  if (!fs["Detector"]["GrayThreshold"].empty())
    fs["Detector"]["GrayThreshold"] >> gray_threshold_;

  if (!fs["Detector"]["MinArea"].empty())
    fs["Detector"]["MinArea"] >> min_area_;

  if (!fs["Detector"]["MinFoundFrame"].empty())
    fs["Detector"]["MinFoundFrame"] >> min_found_frame_;

  std::cout << "[Detector] Config Loaded: "
            << "GrayThreshold=" << gray_threshold_ << ", MinArea=" << min_area_
            << ", MinFoundFrame=" << min_found_frame_ << std::endl;
  return true;
}

// 修复 2: 匹配头文件签名，只留一个参数
void Detector::preprocess(const cv::Mat &input) {
  // 转换为灰度图
  cv::cvtColor(input, gray_, cv::COLOR_BGR2GRAY);
  // 高斯滤波平滑噪声
  cv::GaussianBlur(gray_, gray_, cv::Size(5, 5), 0);
  // 极高阈值二值化提取白色核心
  cv::threshold(gray_, mask_, gray_threshold_, 255, cv::THRESH_BINARY);
  // 形态学开运算，去除细小噪点并使核心实心化
  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
  cv::morphologyEx(mask_, mask_, cv::MORPH_OPEN, kernel);
}

// 辅助函数：寻找目标并评分
bool Detector::findTarget(const cv::Mat &input, cv::Point2f &best_center) {
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask_, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
  /*mask_: 这里使用的是类的成员变量 mask_（通常是 preprocess
  函数生成的黑白二值图），而不是传入的 input。

  cv::findContours: OpenCV 的核心函数，用于在黑白图中找白色的连通区域。

  cv::RETR_EXTERNAL:
  关键参数。只检测最外围的轮廓。如果目标内部有空洞（比如甜甜圈），空洞会被忽略，这能减少计算量。

  cv::CHAIN_APPROX_SIMPLE:
  压缩轮廓点。比如一条直线，它只存起点和终点，不存中间所有的点，节省内存。*/

  bool frame_found = false;
  double max_score = -1.0;

  for (const auto &contour : contours) {
    double area = cv::contourArea(contour);
    if (area < min_area_)
      continue; // 面积过滤（200<)

    double perimeter = cv::arcLength(contour, true);
    double circularity =
        (4 * CV_PI * area) /
        (perimeter * perimeter); // perimeter: 轮廓的周长 circularity: 圆度公式
    /*对于完美的圆，结果是 1.0。

    对于正方形，结果约 0.785。

    形状越细长或边缘越参差不齐，这个值越接近 0。*/

    cv::RotatedRect min_rect = cv::minAreaRect(contour);
    float w = min_rect.size.width;
    float h = min_rect.size.height;
    float ratio = (w > h) ? (w / h) : (h / w);
    /*minAreaRect:
    给轮廓套一个最小外接旋转矩形（即兴的矩形，可以斜着框住物体）。

    ratio: 计算长宽比。无论宽大还是高大，永远用 长边 / 短边，确保 ratio
    >= 1.0。正方形或圆形的 ratio 接近 1.0。*/

    // 核心评分逻辑
    if (circularity > 0.5 && ratio < 1.8) {
      double score = (circularity * 10.0) + (10.0 / ratio) + (area / 100.0);
      if (score > max_score) {
        max_score = score;
        best_center = min_rect.center;
        frame_found = true;
      }
    }
    /*初筛：只看那些“比较圆”（circularity > 0.5）且“不太长”（ratio
    < 1.8）的轮廓。

    评分公式：这是一个加权打分系统：

    circularity * 10.0：越圆分越高。

    10.0 / ratio：长宽比越接近 1（越像正方形/圆形），分越高。

    area /
    100.0：面积越大分越高（优先锁定离摄像头近的、大的目标，而不是远处的小噪点）。

    更新最佳结果：如果当前轮廓分数超过了历史最高分，它就暂时成为“最佳目标”，记录它的中心点
    min_rect.center。*/
  }
  return frame_found;
}

// 修复 3: 加上 const，确保签名与头文件完全一致
DetectionResult Detector::process(const cv::Mat &frame) {
  preprocess(frame);

  cv::Point2f current_center(0, 0);
  bool frame_found = findTarget(frame, current_center);

  if (frame_found) {
    found_count_++;
  } else {
    found_count_ = 0;
  }
  /*这是一个非常重要的防抖动设计。

  如果这一帧看到了目标，计数器 found_count_ 加 1。

  如果这一帧没看到（哪怕只是偶尔闪烁丢了一帧），计数器直接清零。这意味着目标必须连续出现，才会被认为是稳定的。*/

  DetectionResult result;
  result.center = current_center;
  result.is_locked = (found_count_ >= min_found_frame_);
  /*is_locked: 只有当连续看到的次数 found_count_
  超过阈值（min_found_frame_，比如设为 5 帧）时，
  才认为目标被锁定了。这避免了因为噪点偶尔闪现导致的误识别。*/

  // 注意：这里需要 frame.cols，所以 preprocess 里的参数是必要的
  result.error_x = current_center.x - (frame.cols / 2.0f);
  /*error_x: 计算偏航角误差。

  current_center.x 是目标在画面中的 X 坐标。

  frame.cols / 2.0f 是画面中心的 X 坐标。

  结果为负数表示目标在画面左边，正数表示在右边。这个值通常直接传给 PID
  控制器用来控制机器人的转向。*/

  return result;
}