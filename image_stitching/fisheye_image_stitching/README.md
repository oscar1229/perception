# 鱼眼图像拼接 (Fisheye Image Stitching)

基于多路鱼眼相机的图像拼接模块，用于生成车辆/机器人周视全景图像（Surround View）。程序从摄像头（或回退图片）读取四路图像，经碗面拼接后实时渲染 3D 全景与 2D 展开视图；无显示器时可离屏渲染并将最后一帧保存为图片。

## 功能简介

- 鱼眼相机标定：内参、畸变系数求解
- 去畸变与投影变换：将鱼眼图像映射到统一的2D鸟瞰图以及3D图
- 车模文件渲染：加载车模3D文件，采用GPU虚拟视点渲染出车模
- 拼接融合：重叠区域的拼接缝平滑过渡处理
- 全景输出：采用GPU渲染的方式生成拼接结果，支持两种渲染模式：离屏渲染，直接渲染到屏幕

## 目录结构

```
sv_avm_lib/
├── CMakeLists.txt          # 编译脚本
├── config.json             # 运行配置（唯一配置入口）
├── run_live_vi.sh          # 启动脚本
├── lib/                    # 预编译动态库
│   ├── libsvrender.so
│   ├── libsvmcalibrate.so
│   ├── libsvmparam.so
│   ├── libmpp.so*
│   └── libvi_k3_cam_plugin.so
├── include/                # 头文件（含 mpp/、glm/ 等依赖）
└── sv_avm_test/
    ├── _aParam.xml         # 标定参数
    ├── sv_avm_render_main_test.cpp
    └── res/                # 车模 + 各通道回退图片
        ├── concept_BUS cycles.dae
        ├── imagech0.jpg
        ├── imagech1.jpg
        ├── imagech2.jpg
        └── imagech3.jpg

```

## 环境依赖

**OpenCV（spacemit 定制版）**

动态库已链接 opencv-spacemit 4.14，必须安装同版本运行时：

```bash
sudo apt install opencv-spacemit=4.14.0-2bb4
```

> **注意**：预编译库固定链接至 `opencv-spacemit 4.14.0-2bb4`，安装其他版本会导致符号版本不匹配，启动时报 `symbol lookup error`。

**其余依赖**

```bash
sudo apt install libgoogle-glog-dev libassimp-dev \
                 libx11-dev libegl-dev libgles2

```

## 编译

```bash
mkdir -p build && cd build
cmake ..
make -j8
```

库的搜索路径已写入可执行文件的 RPATH，无需设置 `LD_LIBRARY_PATH`。

---

## 运行

在 **sv_avm_lib 根目录**执行启动脚本（脚本会自动进入 `build/` 并运行可执行文件）：

```bash
./run_live_vi.sh
```

可通过命令行参数临时覆盖 `config.json` 中的配置：

```bash
./run_live_vi.sh --frames 100
```

按 **Ctrl+C** 终止；若开启了离屏渲染，退出时自动保存最后一帧。

---

## 配置说明

所有运行参数统一在 **sv_avm_lib 根目录**的 `config.json` 中配置，程序以此为唯一来源。`build/` 目录下不存在也不需要配置文件副本。

| 配置项 | 类型 | 说明 |
|---|---|---|
| `live_vi` | bool | 启用摄像头输入。摄像头不可用时自动回退为图片 |
| `frames` | int | 渲染帧数，`0` 表示持续运行直到 Ctrl+C |
| `sleep_us` | int | 每帧节流睡眠，单位微秒，`0` 表示不限速 |
| `grid_subdiv` | int | 碗面网格细分数，可选 `90 / 180 / 360 / 720`，值越大越精细 |
| `live_vi_dev` | int | VI 设备编号 |
| `live_vi_width` / `live_vi_height` | int | 摄像头分辨率（像素） |
| `live_vi_timeout_ms` | int | 单帧采集超时（毫秒） |
| `live_vi_mipi_lanes` | int | MIPI 通道数 |
| `live_vi_mbps` | int | MIPI 带宽（Mbps） |
| `use_fallback_image` | bool | `true` 强制使用图片输入，跳过摄像头初始化（调试用） |
| `fallback_image_dir` | string | 回退图片目录，相对工程根目录，默认 `sv_avm_test/res` |
| `force_offscreen` | bool | `true` 强制离屏渲染，即使显示器可用（需同时配置 `offscreen_output_path`） |
| `offscreen_output_path` | string | 离屏渲染结果保存路径，相对工程根目录，有显示器时忽略 |

---

## 输入源与回退逻辑

- `live_vi: true`：程序先尝试打开摄像头，失败时自动读取 `fallback_image_dir` 下的图片。
- `live_vi: false` 或 `use_fallback_image: true`：直接读取图片，跳过摄像头初始化。

`fallback_image_dir` 目录下须包含四张图片，分别对应左、右、前、后四路摄像头：

```
imagech0.jpg   imagech1.jpg   imagech2.jpg   imagech3.jpg
```

---

## 显示器与离屏渲染

程序会自动检测显示器是否可用：

- **有显示器**：正常渲染到屏幕，`offscreen_output_path` 被忽略。
- **无显示器**：自动切换为 EGL Pbuffer 离屏渲染，退出时将最后一帧保存到配置路径，输出目录不存在时自动创建。

> **注意**：关闭显示器电源不等于无显示器。X Server 仍在运行时，程序会尝试创建窗口表面，创建失败才退化为离屏模式。需要主动强制离屏渲染时，请将 `force_offscreen` 设为 `true`。

---

## 典型场景

**摄像头 + 显示器（默认）**

保持默认配置，直接运行：

```bash
./run_live_vi.sh
```

**无摄像头，用图片调试**

```json
"use_fallback_image": true,
"fallback_image_dir": "sv_avm_test/res"
```

**无显示器，离屏渲染保存图片**

```json
"force_offscreen": true,
"offscreen_output_path": "output/render.jpg"
```

**固定帧数渲染（用于自动化测试）**

```json
"frames": 200,
"use_fallback_image": true,
"force_offscreen": true,
"offscreen_output_path": "output/test_frame.jpg"
```


## 参数说明

- `_aParam.xml`：各相机内参，畸变系数以及外参
- `concept_BUS cycles.dae`：车模文件
- `imagech0.jpg-imagech3.jpg`：对应左、右、前、后四路鱼眼相机，分辨率 1280x720


## 注意事项

- 保证各个apriltag在相机重叠区域都能完整的被拍摄到
- 每个相机画面需要拍摄到两个apriltag
- 每个相机的水平fov在180度左右


## 性能指标

测试条件：4 路 1280x720 鱼眼输入，Release 构建。
输出条件：1920x1080输出

| 指标 | 2D 鸟瞰模式 + 3D 渲染模式 |
|------|--------------------------|
| 帧率 离屏渲染(FPS) | 110 |
| 帧率 直接渲染(FPS) | 60   |




