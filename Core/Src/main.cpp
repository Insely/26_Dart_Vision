#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

// 在 main 函数外部定义一些全局变量
int found_count = 0;           // 连续检测到的帧数
const int threshold_found = 3; // 连续检测到 3 帧才锁定
bool is_locked = false;        // 是否锁定目标

int main()
{
    VideoCapture cap("/home/dart/code/DartDemo/demo.mp4");
    if (!cap.isOpened())
        return -1;

    // 创建展示窗口
    namedWindow("demo", WINDOW_NORMAL);
    namedWindow("Grayscale", WINDOW_NORMAL); // 灰度窗口
    namedWindow("Binarized", WINDOW_NORMAL); // 二值化窗口

    // 2. 调整窗口到指定尺寸（宽度, 高度）
    // 注意：这会改变窗口大小，但图像可能会按比例缩放或拉伸
    resizeWindow("demo", 800, 600);
    resizeWindow("Grayscale", 800, 600);
    resizeWindow("Binarized", 800, 600);

    Mat frame, dimmed, gray, mask;

    while (true)
    {
        cap >> frame;
        if (frame.empty())
            break;

        // --- 0. 保存干净原图的逻辑 ---
        // 必须在所有 draw 操作（rectangle/circle）之前判断，参考 Track.cpp 的原始数据保存思路
        int key = waitKey(30);
        if (key == 's')
        {
            static int img_count = 0;
            imwrite("sample_" + to_string(img_count++) + ".jpg", frame);
            cout << "干净样本已保存" << endl;
        }
        else if (key == 27)
        { // ESC 退出
            break;
        }

        // -- -1. 获取原始灰度图-- -
        // 不再使用 convertTo 降低亮度，保留原始的高像素值
        cvtColor(frame, gray, COLOR_BGR2GRAY);

        // --- 2. 高斯滤波（新增） ---
        // 对灰度图进行轻微模糊，消除传感器噪点，让白色核心边缘更平滑
        GaussianBlur(gray, gray, Size(5, 5), 0);

        // --- 3. 极高阈值二值化 ---
        // 直接寻找图像中灰度值在 250-255 之间的部分（即白色核心）
        // 这样不需要模拟降曝，因为高阈值本身就是一种“亮度过滤”
        threshold(gray, mask, 250, 255, THRESH_BINARY);

        // --- 4. 形态学闭运算（新增） ---
        // 填补白色核心内部可能存在的微小黑点，使其成为实心圆块
        // 在 threshold 之后，findContours 之前
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5)); // 5x5 的圆核
        morphologyEx(mask, mask, MORPH_OPEN, kernel);

        // 如果想进一步让圆点更丰满，可以再加一个膨胀
        // morphologyEx(mask, mask, MORPH_DILATE, kernel);

        // --- 5. 提供 imshow 观察 ---
        // imshow("Grayscale_Target", gray); // 观察是否只有中心点是亮的
        // imshow("Binarized_Core", mask);   // 观察是否得到了一个干净的小圆点

        // --- 4. 目标检测与偏差计算 (改良版：加入面积权重) ---
        Mat display_frame = frame.clone();
        vector<vector<Point>> contours;
        findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        // 状态控制变量
        static int found_count = 0;
        const int min_found_frame = 3;
        bool frame_found = false;

        Point2f current_best_center(0, 0);
        double max_score = -1.0;

        for (const auto &contour : contours)
        {
            double area = contourArea(contour);
            // 【修改点 1】上调最小面积门槛，直接把细小噪点在第一关就拦住
            // 如果你的灯在画面中比较大，建议把 10 改为 50 甚至更高
            if (area < 200)
                continue;

            double perimeter = arcLength(contour, true);
            double circularity = (4 * CV_PI * area) / (perimeter * perimeter);

            RotatedRect min_rect = minAreaRect(contour);
            float w = min_rect.size.width;
            float h = min_rect.size.height;
            float ratio = (w > h) ? (w / h) : (h / w);

            // 稍微放宽门槛，允许有一定形变的大圆进入评分环节
            if (circularity > 0.5 && ratio < 1.8)
            {
                // 【修改点 2】核心评分公式：增加面积权重 (area / 100.0)
                // 这样即使噪点圆度是 1.0，只要它面积小，总分也绝对比不过大圆
                double score = (circularity * 10.0) + (10.0 / ratio) + (area / 100.0);

                if (score > max_score)
                {
                    max_score = score;
                    current_best_center = min_rect.center;
                    frame_found = true;
                }
            }
        }

        // --- 状态机逻辑更新 ---
        if (frame_found)
            found_count++;
        else
            found_count = 0;

        // --- 绘制锁定效果 ---
        if (found_count >= min_found_frame)
        {
            // 只有连续检测到目标才画框
            circle(display_frame, current_best_center, 8, Scalar(0, 0, 255), -1);

            // 计算并打印偏差
            float error_x = current_best_center.x - (frame.cols / 2.0f);
            string info = "LOCKED | X_Err: " + to_string((int)error_x);
            putText(display_frame, info, Point(30, 50), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
        }
        else
        {
            putText(display_frame, "SEARCHING...", Point(30, 50), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 255), 2);
        }

        imshow("demo", display_frame); // 带框的显示图
        imshow("Grayscale", gray);     // 灰度图
        imshow("Binarized", mask);     // 二值化图
    }

    cap.release();
    destroyAllWindows();
    return 0;
}