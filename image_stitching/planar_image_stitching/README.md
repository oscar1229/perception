# 平面图像拼接 (Planar Image Stitching)

基于双目相机的平面图像拼接模块，用于将左右两路视角重叠的图像拼接为一张宽幅全景图。程序从摄像头（或回退图片）读取两路图像，经特征配准、曝光补偿、拼接缝求解后由 GPU 实时渲染；无显示器时可离屏渲染并将最后一帧保存为图片。

## 功能简介

- 特征配准：FAST 特征检测 + RANSAC 求解左右图之间的单应矩阵，支持在线估计与读取标定文件
- 曝光补偿：估计右图相对左图的 RGB 增益与偏置
- 拼接缝求解：Voronoi seam finder 求解重叠区拼接缝
- 多波段融合：拉普拉斯金字塔融合，消除亮度跳变
- 全景输出：支持屏幕显示与离屏渲染

## 目录结构

```text
planar_image_stitching/
├── CMakeLists.txt
├── README.md
├── config.json
├── run_planar.sh
├── lib/
│   └── libplanar_stitcher_core.a
├── include/planar_stitcher/
├── assets/                 # 回退图片
├── output/                 # 运行时生成的图片和标定文件
└── stitcher_test/
    ├── planar_stitcher_main.cpp
    └── calib.xml
```

## MPP 源码

MPP 源码的默认路径是相对于本目录的 `../../../multimedia/mpp/`。如果 MPP 源码位于其他位置，配置编译时指定路径：

```bash
cmake -S . -B build -DMPP_ROOT=/path/to/mpp
cmake --build build -j8
```

MPP 仓库：https://github.com/spacemit-com/mpp.git

## 环境依赖

**OpenCV**

预编译库链接 opencv-spacemit 4.14，运行时需要兼容版本：

```bash
sudo apt install opencv-spacemit=4.14.0-2bb4
```

安装其他版本可能导致 `symbol lookup error`。

**其余依赖**

```bash
sudo apt install libx11-dev libegl-dev libgles2
```

## 构建与运行

在本目录构建示例，然后运行：

```bash
cmake -S . -B build
cmake --build build -j8
./run_planar.sh
```

也可以直接运行：

```bash
./build/planar_stitcher [path/to/config.json]
```

按 Ctrl+C 终止；离屏模式退出时保存最后一帧。

## 配置说明

运行参数统一在本目录的 `config.json` 中配置，也可通过启动脚本指定配置文件。

| 配置项 | 类型 | 说明 |
|---|---|---|
| `input.left_image` / `input.right_image` | string | 回退图片路径 |
| `output.image` | string | 拼接结果保存路径 |
| `camera.enable` | bool | `false` 跳过摄像头直接用图片；`true` 时失败自动回退 |
| `camera.width` / `camera.height` | int | 摄像头和输入图片尺寸 |
| `camera.device` | int | VI 设备编号 |
| `camera.timeout_ms` | int | 单帧采集超时（毫秒） |
| `camera.mipi_lanes` / `camera.mipi_mbps` | int | MIPI 参数 |
| `registration.mode` | string | `auto` 在线估计；`file` 读取标定文件 |
| `registration.file` | string | `mode=file` 时读取的标定文件 |
| `registration.save_to` | string | 保存配准结果的路径 |
| `feature_detection.*` | - | 特征检测参数 |
| `ransac.*` | - | RANSAC 参数 |
| `blending.num_bands` | int | 融合波段数，`0` 自动选择 |
| `runtime.frames` | int | 渲染帧数，`0` 持续运行 |
| `runtime.sleep_us` | int | 每帧节流睡眠（微秒） |
| `runtime.force_offscreen` | bool | `true` 强制离屏渲染 |

## 输入与离屏渲染

- `camera.enable: true`：先尝试摄像头，失败后读取配置中的两张图片。
- `camera.enable: false`：直接读取图片，跳过摄像头初始化。
- 输入尺寸必须与 `camera.width` / `camera.height` 一致。
- 有显示器时渲染到屏幕；无显示器时自动切换为 EGL Pbuffer 离屏渲染。
- 渲染目标固定为 1920x1080；输出目录不存在时自动创建。

## 典型场景

无摄像头，用图片调试：

```json
"camera": { "enable": false },
"input": { "left_image": "./assets/s0_left.jpg", "right_image": "./assets/s0_right.jpg" }
```

无显示器，离屏保存：

```json
"runtime": { "force_offscreen": true },
"output": { "image": "./output/planar.jpg" }
```

固定帧数测试：

```json
"camera": { "enable": false },
"registration": { "mode": "file", "file": "./stitcher_test/calib.xml" },
"runtime": { "frames": 200, "force_offscreen": true }
```

## 性能指标

1080P 输出下的实测帧率：

| 指标 | 帧率 (FPS) |
|---|---|
| 屏幕渲染 | 29 |
| 离屏渲染 | 31 |

以上为既有测量值，实际性能取决于输入尺寸、配准模式、融合波段数和目标硬件。
