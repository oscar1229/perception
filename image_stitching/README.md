# 图像拼接 (Image Stitching)

多相机图像拼接解决方案，包含鱼眼相机环视拼接与平面相机全景拼接两个独立模块。两者均基于 SpacemiT RISC-V 平台，使用 MPP 硬件解码与 GPU 实时渲染。

## 模块对比

| 特性 | 鱼眼拼接 | 平面拼接 |
|------|---------|---------|
| **相机类型** | 鱼眼相机 | 平面相机 |
| **输入路数** | 4 路（前后左右） | 2 路（左右） |
| **典型应用** | 车辆环视、机器人导航 | 前向监控、全景摄像 |
| **投影方式** | 3D 碗面投影 + 2D 鸟瞰 | 平面单应变换 |
| **输入分辨率** | 4 × 1280x720 | 2 × 1920x1080 |
| **输出画布** | 可配置（如 1920x1080） | ~3148x1080（取决于重叠区） |

## 模块详情

### [鱼眼图像拼接](./fisheye_image_stitching/)

基于四路鱼眼相机的车辆环视系统（AVM/Surround View），将前后左右四个摄像头的鱼眼图像去畸变后投影到 3D 碗面，生成车辆周围的全景图像。

**主要功能**
- 鱼眼相机内参标定与畸变矫正
- 3D 碗面投影与 2D 鸟瞰展开
- 车模 3D 渲染（支持 .dae 格式）
- 重叠区域拼接缝平滑融合

**典型场景**
- 汽车泊车辅助系统
- 移动机器人全向感知
- 无人驾驶环境感知

**目录**：`./fisheye_image_stitching/` → [README.md](./fisheye_image_stitching/README.md)

### [平面图像拼接](./planar_image_stitching/)

基于双目平面相机的宽幅全景拼接，通过特征配准求解左右图像间的单应矩阵，使用多波段融合消除拼接缝。

**主要功能**
- FAST 特征检测 + RANSAC 单应矩阵求解
- 曝光补偿（RGB 增益与偏置估计）
- 多波段融合（拉普拉斯金字塔）
- 实时 GPU 渲染（12.9 FPS @ 1080P）

**典型场景**
- 前向宽视角监控
- 全景摄像与录制
- 双目视觉测距前处理

**目录**：`./planar_image_stitching/` → [README.md](./planar_image_stitching/README.md)

## 共同特性

### 硬件加速
- **MPP 解码**：硬件解码
- **GPU 渲染**：OpenGL ES 3.2 着色器渲染，支持离屏与显示模式
- **DMA 零拷贝**：摄像头 → 解码器 → 渲染器全程 DMA 传输

### 摄像头与回退
两个模块都支持自动回退机制：
```json
{
  "camera": {
    "enable": true  // true: 尝试摄像头，失败回退到图片
                    // false: 跳过摄像头，直接使用图片
  }
}
```

### 渲染模式
- **显示模式**：渲染到 X11 窗口，支持 Esc 退出
- **离屏模式**：EGL Pbuffer 离屏渲染，无需显示器

## 快速开始

### 环境准备

```bash
# 安装 OpenCV（版本必须匹配）
sudo apt install opencv-spacemit=4.14.0-2bb4

# 安装其他依赖
sudo apt install libx11-dev libegl-dev libgles2
```

### 鱼眼拼接

```bash
cd fisheye_image_stitching
./run_live_vi.sh          # 使用摄像头（自动回退图片）
```

配置文件：`config.json`，标定文件：`_aParam.xml`

### 平面拼接

```bash
cd planar_image_stitching
./run.sh                  # 使用摄像头（自动回退图片）
```

配置文件：`config.json`，标定文件：`calib.xml`（可选）


## 目录结构

```
image_stitching/
├── README.md                          # 本文档（拼接模块总览）
├── fisheye_image_stitching/           # 鱼眼环视拼接
│   ├── README.md                      # 鱼眼拼接详细文档
│   ├── CMakeLists.txt
│   ├── config.json                    # 鱼眼拼接配置
│   ├── run_live_vi.sh
│   ├── lib/                           # 预编译库
│   │   ├── libsvrender.so
│   │   ├── libsvmcalibrate.so
│   │   └── libmpp.so
│   ├── include/
│   └── sv_avm_test/
│       ├── _aParam.xml                # 鱼眼标定文件
│       └── res/                       # 车模 + 回退图片
└── planar_image_stitching/            # 平面全景拼接
    ├── README.md                      # 平面拼接详细文档
    ├── CMakeLists.txt
    ├── config.json                    # 平面拼接配置
    ├── run.sh
    ├── lib/
    │   ├── libplanar_stitcher_core.a
    │   └── libmpp.so
    ├── include/
    ├── assets/                        # 回退图片
    │   ├── s0_left.jpg
    │   └── s0_right.jpg
    └── stitcher_test/
        └── calib.xml                  # 平面标定文件（可选）
```
