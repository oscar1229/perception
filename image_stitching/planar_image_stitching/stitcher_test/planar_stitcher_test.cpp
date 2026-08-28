// 平面拼接库使用示例
// 本程序使用预编译的 libplanar_stitcher_core.a 静态库
// 演示完整的双目图像拼接流程

#include <planar_stitcher/blend_pipeline.hpp>
#include <planar_stitcher/config.hpp>
#include <planar_stitcher/egl_window.hpp>
#include <planar_stitcher/feature_matcher.hpp>
#include <planar_stitcher/planar_renderer.hpp>
#include <planar_stitcher/mpp_jpeg_decoder.hpp>
#include <planar_stitcher/mpp_vi_capture.hpp>

#include <opencv2/imgcodecs.hpp>
#include <unistd.h>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>

using namespace planar_stitcher;

namespace {
volatile std::sig_atomic_t g_exit = 0;

void HandleSignal(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM) g_exit = 1;
}

int Fail(const char* stage, const std::string& error) {
    std::cerr << stage << ": " << error << "\n";
    return 1;
}
}

int main() {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    std::string error;

    // 1. 加载配置
    Config config;
    if (!ParseConfigFile("config.json", &config, &error)) {
        return Fail("config", error);
    }

    // 2. 初始化解码器并加载图像
    MppJpegDecoder decoder;
    if (!decoder.Open(&error)) {
        return Fail("decoder", error);
    }

    Nv12DmaFrame left, right;
    if (!decoder.DecodeFile(config.left_image, &left, &error)) {
        return Fail("left", error);
    }
    if (!decoder.DecodeFile(config.right_image, &right, &error)) {
        decoder.Release(&left);
        return Fail("right", error);
    }
    std::cout << "input=images " << left.width << "x" << left.height << "\n";

    // 3. 配准
    RegistrationOptions reg_opts;
    reg_opts.work_max_width = config.work_max_width;
    reg_opts.fast_threshold = config.fast_threshold;
    reg_opts.max_features = config.max_features;
    reg_opts.ransac_iterations = config.ransac_iterations;
    reg_opts.ransac_threshold_px = config.ransac_threshold_px;
    reg_opts.max_reprojection_rmse_px = config.max_reprojection_rmse_px;
    reg_opts.registration_mode = config.registration_mode;
    reg_opts.registration_file = config.registration_file;
    reg_opts.registration_save_to = config.registration_save_to;

    LumaView left_luma{left.y_data, left.width, left.height,
                       static_cast<int>(left.y_stride)};
    LumaView right_luma{right.y_data, right.width, right.height,
                        static_cast<int>(right.y_stride)};

    RegistrationResult registration;
    if (!RegisterHomography(left_luma, right_luma, reg_opts,
                           &registration, &error)) {
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("registration", error);
    }

    if (config.registration_mode == "auto") {
        std::cout << "registration=auto " << registration.matches
                  << " matches, " << registration.inliers << " inliers\n";
    } else {
        std::cout << "registration=calib file " << config.registration_file << "\n";
    }

    // 4. 投影和融合准备
    Nv12View left_view{left.y_data, left.uv_data, left.width, left.height,
                       static_cast<int>(left.y_stride),
                       static_cast<int>(left.uv_stride)};
    Nv12View right_view{right.y_data, right.uv_data, right.width, right.height,
                        static_cast<int>(right.y_stride),
                        static_cast<int>(right.uv_stride)};

    RgbGain gain = EstimateRightRgbGain(left_view, right_view,
                                        registration.right_to_left);
    ExposureModel exposure;
    exposure.gain[0] = gain.r;
    exposure.gain[1] = gain.g;
    exposure.gain[2] = gain.b;
    exposure.bias[0] = gain.bias_r;
    exposure.bias[1] = gain.bias_g;
    exposure.bias[2] = gain.bias_b;

    ProjectedBlendPair projected;
    if (!ProjectPlanarPair(left_view, right_view, registration, exposure,
                          &projected, &error)) {
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("project", error);
    }

    int bands = config.num_bands;
    if (bands == 0) {
        bands = ChooseBlendBands(projected.left.width, projected.left.height,
                                 registration.planar_bounds.width);
    }

    BlendMaskPyramid masks;
    if (!BuildGraphCutMaskPyramid(projected, bands, &masks, &error)) {
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("blend_prep", error);
    }

    std::cout << "seam_overlap_width=" << masks.overlap_width
              << " blend_bands=" << bands << "\n";

    // 5. 初始化 EGL 和渲染器
    EglWindow window;
    if (!window.Open(1920, 1080, "Planar Stitcher", config.force_offscreen, &error)) {
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("egl", error);
    }

    std::cout << "render_target=" << (window.is_offscreen() ? "offscreen" : "display")
              << " frames=" << config.frames << "\n";

    PlanarRenderer renderer;
    if (!renderer.Initialize(window, &error) ||
        !renderer.Prepare(registration, masks, &error)) {
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("renderer", error);
    }

    // 6. 渲染循环：跑满 config.frames 帧，或直到 Ctrl+C /
    //    （上屏时）窗口请求退出。frames <= 0 表示不限帧数。
    using Clock = std::chrono::steady_clock;
    uint64_t frames = 0;
    double total_gpu_ms = 0.0;
    const auto run_start = Clock::now();
    auto report_start = run_start;
    uint64_t report_frames = 0;
    double report_gpu_ms = 0.0;
    if (config.frames <= 0) {
        std::cout << "frames=0: rendering until Ctrl+C"
                  << (window.is_offscreen() ? "" : " (or Esc / window close)")
                  << "\n";
    }
    while (g_exit == 0 &&
           (config.frames <= 0 ||
            frames < static_cast<uint64_t>(config.frames))) {
        const auto frame_start = Clock::now();
        if (!renderer.RenderFrame(left, right, exposure, &error)) {
            decoder.Release(&left);
            decoder.Release(&right);
            return Fail("render", error);
        }
        const double gpu_ms = std::chrono::duration<double, std::milli>(
            Clock::now() - frame_start).count();
        ++frames;
        ++report_frames;
        total_gpu_ms += gpu_ms;
        report_gpu_ms += gpu_ms;

        // 上屏渲染靠 Present() 交换缓冲区，离屏时不需要。
        if (!window.is_offscreen() && !renderer.Present(&error)) {
            decoder.Release(&left);
            decoder.Release(&right);
            return Fail("present", error);
        }

        const auto now = Clock::now();
        const double report_seconds =
            std::chrono::duration<double>(now - report_start).count();
        if (report_seconds >= 1.0) {
            std::cout << std::fixed << std::setprecision(2)
                      << "stitch_fps=" << report_frames / report_seconds
                      << " avg_gpu_ms=" << report_gpu_ms / report_frames
                      << " bands=" << bands << "\n";
            report_start = now;
            report_frames = 0;
            report_gpu_ms = 0.0;
        }
        if (config.sleep_us > 0) {
            usleep(static_cast<useconds_t>(config.sleep_us));
        }
        if (window.PollExitRequested()) break;
    }
    const double run_seconds =
        std::chrono::duration<double>(Clock::now() - run_start).count();

    if (frames == 0) {
        // 首帧之前就被中断，没有可保存的内容。
        std::cout << "stitch_frames=0 (interrupted before first frame)\n";
        decoder.Release(&left);
        decoder.Release(&right);
        return 0;
    }

    std::cout << std::fixed << std::setprecision(2)
              << "stitch_frames=" << frames
              << " avg_fps=" << (run_seconds > 0.0 ? frames / run_seconds : 0.0)
              << " avg_gpu_ms=" << (total_gpu_ms / frames)
              << " bands=" << bands << "\n";

    // 7. 回读并保存
    std::vector<uint8_t> bgr;
    if (!renderer.Readback(&bgr, &error)) {
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("readback", error);
    }

    const std::filesystem::path output(config.output);
    if (!output.parent_path().empty()) {
        std::filesystem::create_directories(output.parent_path());
    }

    cv::Mat image(renderer.height(), renderer.width(), CV_8UC3, bgr.data());
    if (!cv::imwrite(config.output, image)) {
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("save", config.output);
    }

    std::cout << "saved=" << config.output << "\n";

    // 8. 清理
    decoder.Release(&left);
    decoder.Release(&right);

    return 0;
}
