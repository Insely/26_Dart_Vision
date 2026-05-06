#include "detector/detector.hpp"

Detector::Detector() : found_count_(0) {}

bool Detector::init(const std::string &config_path) {
  if (config_path.empty())
    return false;

  cv::FileStorage fs(config_path, cv::FileStorage::READ);
  if (!fs.isOpened()) {
    std::cerr << "Detector Config file not found: " << config_path << std::endl;
    return false;
  }

  if (!fs["Detector"]["H_min"].empty())
    fs["Detector"]["H_min"] >> h_min_;
  if (!fs["Detector"]["H_max"].empty())
    fs["Detector"]["H_max"] >> h_max_;
  if (!fs["Detector"]["S_min"].empty())
    fs["Detector"]["S_min"] >> s_min_;
  if (!fs["Detector"]["S_max"].empty())
    fs["Detector"]["S_max"] >> s_max_;
  if (!fs["Detector"]["V_min"].empty())
    fs["Detector"]["V_min"] >> v_min_;
  if (!fs["Detector"]["V_max"].empty())
    fs["Detector"]["V_max"] >> v_max_;

  if (!fs["Detector"]["MinArea"].empty())
    fs["Detector"]["MinArea"] >> min_area_;

  if (!fs["Detector"]["MinFoundFrame"].empty())
    fs["Detector"]["MinFoundFrame"] >> min_found_frame_;

  std::cout << "[Detector] Config Loaded: "
            << "H=" << h_min_ << "-" << h_max_ << ", S=" << s_min_ << "-"
            << s_max_ << ", V=" << v_min_ << "-" << v_max_
            << ", MinArea=" << min_area_
            << ", MinFoundFrame=" << min_found_frame_ << std::endl;
  return true;
}

void Detector::preprocess(const cv::Mat &input) {
  // 仅做 BGR → HSV 转换，不做全局二值化
  cv::cvtColor(input, hsv_, cv::COLOR_BGR2HSV);
}

bool Detector::findTarget(const cv::Mat &input, cv::Point2f &best_center) {
  // ====== 第一阶段：粗筛 ======
  // 在全图做 inRange 快速找出绿色像素（无形态学处理，开销低）
  cv::Mat rough_mask;
  cv::inRange(hsv_, cv::Scalar(h_min_, s_min_, v_min_),
              cv::Scalar(h_max_, s_max_, v_max_), rough_mask);

  // 在粗糙 mask 上找轮廓，只保留大色块
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(rough_mask, contours, cv::RETR_EXTERNAL,
                   cv::CHAIN_APPROX_SIMPLE);

  // 在大色块中选出最圆的一个
  int best_idx = -1;
  double best_circularity = -1.0;

  for (int i = 0; i < (int)contours.size(); i++) {
    double area = cv::contourArea(contours[i]);
    if (area < min_area_)
      continue;

    double perimeter = cv::arcLength(contours[i], true);
    if (perimeter < 1e-5)
      continue;
    double circularity = (4 * CV_PI * area) / (perimeter * perimeter);

    if (circularity > best_circularity) {
      best_circularity = circularity;
      best_idx = i;
    }
  }

  // 初始化 debug mask 为全黑
  mask_ = cv::Mat::zeros(input.rows, input.cols, CV_8UC1);

  if (best_idx == -1)
    return false;

  // ====== 第二阶段：ROI 精处理 ======
  // 用选中色块的外接矩形裁切 ROI（加 padding 防止边缘截断）
  cv::Rect bbox = cv::boundingRect(contours[best_idx]);
  int pad = 20;
  int roi_x = std::max(0, bbox.x - pad);
  int roi_y = std::max(0, bbox.y - pad);
  int roi_w = std::min(input.cols - roi_x, bbox.width + 2 * pad);
  int roi_h = std::min(input.rows - roi_y, bbox.height + 2 * pad);
  cv::Rect roi_rect(roi_x, roi_y, roi_w, roi_h);

  // 只对 ROI 区域做精确二值化 + 形态学
  cv::Mat roi_hsv = hsv_(roi_rect);
  cv::Mat roi_mask;
  cv::inRange(roi_hsv, cv::Scalar(h_min_, s_min_, v_min_),
              cv::Scalar(h_max_, s_max_, v_max_), roi_mask);

  cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
  cv::morphologyEx(roi_mask, roi_mask, cv::MORPH_OPEN, kernel);

  // 将 ROI mask 写回全图 mask_（用于 debug 显示）
  roi_mask.copyTo(mask_(roi_rect));

  // 在 ROI mask 中找精确轮廓，选最圆的作为最终目标
  std::vector<std::vector<cv::Point>> roi_contours;
  cv::findContours(roi_mask, roi_contours, cv::RETR_EXTERNAL,
                   cv::CHAIN_APPROX_SIMPLE);

  bool found = false;
  double max_circ = -1.0;

  for (const auto &c : roi_contours) {
    double area = cv::contourArea(c);
    if (area < min_area_)
      continue;

    double perimeter = cv::arcLength(c, true);
    if (perimeter < 1e-5)
      continue;
    double circularity = (4 * CV_PI * area) / (perimeter * perimeter);

    if (circularity > max_circ) {
      max_circ = circularity;
      // ROI 内坐标 → 全图坐标（用矩的质心，比外接矩形中心更精确）
      cv::Moments m = cv::moments(c);
      if (m.m00 > 0) {
        best_center.x = static_cast<float>(m.m10 / m.m00) + roi_x;
        best_center.y = static_cast<float>(m.m01 / m.m00) + roi_y;
        found = true;
      }
    }
  }

  return found;
}

DetectionResult Detector::process(const cv::Mat &frame) {
  preprocess(frame);

  cv::Point2f current_center(0, 0);
  bool frame_found = findTarget(frame, current_center);

  if (frame_found) {
    found_count_++;
  } else {
    found_count_ = 0;
  }

  DetectionResult result;
  result.center = current_center;
  result.is_locked = (found_count_ >= min_found_frame_);
  result.error_x = current_center.x - (frame.cols / 2.0f);

  return result;
}
