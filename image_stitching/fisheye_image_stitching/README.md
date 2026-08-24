# 鱼眼图像拼接 (Fisheye Image Stitching)

基于多路鱼眼相机的图像拼接模块，用于生成车辆/机器人周视全景图像（Surround View）。

## 功能简介

- 鱼眼相机标定：内参、畸变系数求解
- 去畸变与投影变换：将鱼眼图像映射到统一的鸟瞰或柱面坐标系
- 多路图像配准：外参标定，确定各相机之间的相对位姿
- 拼接融合：重叠区域的接缝处理与亮度/色彩一致性调整
- 全景输出：生成实时可用的拼接结果

## 目录结构

```
fisheye_image_stitching/
├── include/          # 头文件
├── src/              # 源码实现
├── config/           # 相机参数与标定配置
├── data/             # 测试图像与标定板样例
├── test/             # 单元测试与示例程序
└── CMakeLists.txt    # 构建脚本
```

## 依赖

| 依赖项 | 版本要求 | 说明 |
|--------|----------|------|
| OpenCV | >= 4.5 | 图像处理、标定、重映射 |
| CMake  | >= 3.16 | 构建系统 |
| C++    | C++17   | 编译标准 |

## 构建

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 使用方式

### 1. 相机标定

采集标定板图像后运行标定程序，生成内参文件：

```bash
./fisheye_calibrate --input data/calib_images --output config/intrinsics.yaml
```

### 2. 外参标定

标定各相机之间的相对位姿：

```bash
./fisheye_extrinsic --config config/intrinsics.yaml --output config/extrinsics.yaml
```

### 3. 图像拼接

```bash
./fisheye_stitching --config config/ --input data/frames --output result/
```

## 参数说明

主要配置项位于 `config/` 目录：

- `intrinsics.yaml`：各相机内参与畸变系数
- `extrinsics.yaml`：相机外参（旋转、平移）
- `stitching.yaml`：拼接参数（输出分辨率、投影模型、融合方式）

## 注意事项

- 鱼眼相机 FOV 较大时，建议使用 `cv::fisheye` 模型而非普通针孔模型
- 标定图像需覆盖画面各个区域，尤其是边缘畸变较大的部分
- 重叠区域应保证至少 15% 以上，以获得稳定的配准结果

## TODO

- [ ] 支持 GPU 加速重映射
- [ ] 在线动态标定
- [ ] 多分辨率金字塔融合
