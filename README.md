# 飞镖自瞄 DartVision

## 启动方法

```bash
# 首次编译并启动（无 UI）
./run.sh -r

# 编译并启动（带相机画面 imshow）
./run.sh -rs

# 已编译过，直接启动（无 UI）
./run.sh

# 已编译过，带相机画面启动
./run.sh -s
```

## 参数说明

| 参数 | 作用 |
| ---- | ---- |
| `-r` | 重新编译项目 |
| `-s` | 启用相机画面 imshow 窗口 |

## 配置文件

配置文件位于 `configs/` 目录：

- `settings.yaml` — 全局设置（串口、显示开关、demo 模式等）
- `camera.yaml` — 相机参数（曝光、增益、Gamma）
- `detector.yaml` — 检测器参数（HSV 阈值、最小面积）

### 显示窗口控制

默认启动不显示任何 imshow 窗口。

- **相机画面**：通过启动参数 `-s` 开启，或在 `settings.yaml` 中设置 `ShowUI: 1`
- **二值化图**：在 `settings.yaml` 中设置 `ShowBinarized: 1`

## ！！！调整yaw轴偏置！！！

在飞镖能够打正前哨战装甲板后，./run.sh -s打开自瞄，遥控器改为手动控制模式（防止yaw轴移动），记住左上角的yaw_error,填入config/settings.yaml里面的YawOffset值，是什么就填什么。