# 平面图像拼接 (Planar Image Stitching)

基于双目相机的平面图像拼接模块，用于将左右两路视角重叠的图像拼接为一张宽幅全景图。程序从摄像头（或回退图片）读取两路图像，经特征配准、曝光补偿、拼接缝求解后由 GPU 实时渲染；无显示器时可离屏渲染并将最后一帧保存为图片。

## 功能简介

- 特征配准：FAST 特征检测 + RANSAC 求解左右图之间的单应矩阵，支持在线估计与读取标定文件两种模式
- 曝光补偿：估计右图相对左图的 RGB 增益与偏置，消除两路相机的亮度/色温差异
- 拼接缝求解：Voronoi seam finder 在重叠区内求解拼接缝，生成多波段融合掩膜金字塔
- 多波段融合：拉普拉斯金字塔融合，消除拼接缝两侧的亮度跳变
- 全景输出：采用 GPU 渲染生成拼接结果，支持两种渲染模式：离屏渲染，直接渲染到屏幕

## 目录结构

```
stitcher_lib/
├── CMakeLists.txt                    # 编译脚本
├── config.json                       # 运行配置（唯一配置入口）
├── run.sh                            # 启动脚本
├── lib/                              # 预编译库
│   ├── libplanar_stitcher_core.a     # 拼接核心静态库
│   └── libmpp.so*                    # MPP 硬件编解码库
├── include/                          # 头文件
│   ├── planar_stitcher/              # 拼接库 API
│   └── mpp/                          # MPP API
├── assets/                           # 回退图片
│   ├── s0_left.jpg
│   └── s0_right.jpg
└── stitcher_test/
    ├── planar_stitcher_test.cpp      # 完整的 API 调用示例
    └── calib.xml                     # 预生成的标定文件
```

## 环境依赖

**OpenCV**

预编译库已链接 opencv-spacemit 4.14，必须安装同版本运行时：

```bash
sudo apt install opencv-spacemit=4.14.0-2bb4
```

> **注意**：安装其他版本会导致符号版本不匹配，启动时报 `symbol lookup error`。

**其余依赖**

```bash
sudo apt install libx11-dev libegl-dev libgles2
```

## 编译

```bash
cmake -B build -S .
cmake --build build -j8
```

库的搜索路径已写入可执行文件的 RPATH，无需设置 `LD_LIBRARY_PATH`。

---

## 运行

在 **stitcher_lib 根目录**执行启动脚本：

```bash
./run.sh
```

按 **Ctrl+C** 终止；退出时自动将最后一帧保存到 `output.image` 配置的路径，输出目录不存在时自动创建。

---

## 配置说明

所有运行参数统一在 **stitcher_lib 根目录**的 `config.json` 中配置，程序以此为唯一来源。

| 配置项 | 类型 | 说明 |
|---|---|---|
| `input.left_image` / `input.right_image` | string | 回退图片路径，摄像头关闭或不可用时使用 |
| `output.image` | string | 拼接结果保存路径 |
| `camera.enable` | bool | 启用摄像头输入。`false` 跳过摄像头初始化直接用回退图片；`true` 时摄像头不可用会自动回退 |
| `camera.width` / `camera.height` | int | 摄像头分辨率（像素），同时用于校验输入图片尺寸 |
| `camera.device` | int | VI 设备编号 |
| `camera.timeout_ms` | int | 单帧采集超时（毫秒） |
| `camera.mipi_lanes` | int | MIPI 通道数 |
| `camera.mipi_mbps` | int | MIPI 带宽（Mbps） |
| `registration.mode` | string | `"auto"` 在线估计单应矩阵；`"file"` 从标定文件读取，跳过特征检测 |
| `registration.file` | string | `mode="file"` 时读取的标定文件路径 |
| `registration.save_to` | string | 非空时将配准结果保存到此路径，父目录不存在时自动创建 |
| `feature_detection.work_max_width` | int | 特征检测降采样宽度，越小越快、精度越低 |
| `feature_detection.fast_threshold` | int | FAST 角点响应阈值 |
| `feature_detection.max_features` | int | 每图最大特征点数 |
| `ransac.iterations` | int | RANSAC 迭代次数 |
| `ransac.threshold_px` | double | RANSAC 内点判定阈值（像素） |
| `ransac.max_reprojection_rmse_px` | double | 配准结果验收阈值，重投影 RMSE 超过则判定配准失败 |
| `blending.num_bands` | int | 多波段融合波段数，`0` 表示按重叠区宽度自动选择（3~5） |
| `runtime.frames` | int | 渲染帧数，`0` 表示持续运行直到 Ctrl+C |
| `runtime.sleep_us` | int | 每帧节流睡眠，单位微秒，`0` 表示不限速 |
| `runtime.force_offscreen` | bool | `true` 强制离屏渲染，即使显示器可用 |

---

## 输入源与回退逻辑

- `camera.enable: true`：程序先尝试打开摄像头，失败时自动回退为 `input.left_image` / `input.right_image` 指定的图片。
- `camera.enable: false`：直接读取图片，跳过摄像头初始化。

无论走哪条路径，输入尺寸都必须与 `camera.width` / `camera.height` 一致，否则报错退出。

---

## 显示器与离屏渲染

程序会自动检测显示器是否可用：

- **有显示器**：渲染到屏幕，每帧交换缓冲区，支持 Esc / 关闭窗口退出。
- **无显示器**：自动切换为 EGL Pbuffer 离屏渲染。

程序会先尝试 `DISPLAY` 环境变量指定的显示器，未设置时自动回退尝试 `:0`，因此 SSH 会话下无需手动 `export DISPLAY=:0`。

> **注意**：关闭显示器电源不等于无显示器。X Server 仍在运行时，程序会尝试创建窗口表面，创建失败才退化为离屏模式。需要主动强制离屏渲染时，请将 `force_offscreen` 设为 `true`。

---

## 典型场景

**摄像头 + 显示器（默认）**

```json
"camera": { "enable": true },
"runtime": { "frames": 0, "force_offscreen": false }
```

**无摄像头，用图片调试**

```json
"camera": { "enable": false },
"input": { "left_image": "./assets/s0_left.jpg", "right_image": "./assets/s0_right.jpg" }
```

**无显示器，离屏渲染保存图片**

```json
"runtime": { "force_offscreen": true },
"output": { "image": "./out/planar.jpg" }
```

**固定帧数渲染（用于自动化测试）**

```json
"camera": { "enable": false },
"registration": { "mode": "file", "file": "./out/calib.xml" },
"runtime": { "frames": 200, "force_offscreen": true }
```

---

## 性能指标

测试条件：2 路 1920x1080 输入，`registration.mode="file"`（配准不计入帧率），`num_bands=5`，Release 构建（`-O3`）。
输出条件：画布 3148x1080，重叠区宽度 692 px。

| 指标 | 平面拼接 |
|------|---------|
| 帧率 离屏渲染(FPS) | 12.9 |
| 帧率 直接渲染(FPS) | 12.1 |


### 影响性能的参数

拼接的每帧开销基本由**画布像素总量**决定，实测约 **23 ms/Mpix**。

测试硬件：SpacemiT X100（RISC-V，8 核）+ PowerVR B-Series BXM-4-64，OpenGL ES 3.2 (Mesa 24.2)，内核 6.18.3，opencv-spacemit 4.14.0，MPP 硬件解码。
