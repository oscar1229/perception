#include "planar_stitcher/blend_pipeline.hpp"
#include "planar_stitcher/config.hpp"
#include "planar_stitcher/egl_window.hpp"
#include "planar_stitcher/feature_matcher.hpp"
#include "planar_stitcher/planar_renderer.hpp"
#include "planar_stitcher/mpp_jpeg_decoder.hpp"
#include "planar_stitcher/mpp_vi_capture.hpp"

#include <opencv2/imgcodecs.hpp>

#include <csignal>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>  // NOLINT(build/c++17)
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>

using namespace planar_stitcher;  // NOLINT(build/namespaces)

namespace {

volatile std::sig_atomic_t g_exit = 0;

void HandleSignal(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM) g_exit = 1;
}

int Fail(const char* stage, const std::string& error) {
    std::cerr << stage << ": " << error << '\n';
    return 1;
}


}  // namespace

int main(int argc, char** argv) {
    const char* config_path = (argc == 2) ? argv[1] : "config.json";
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);
    Config config;
    std::string error;
    if (!ParseConfigFile(config_path, &config, &error)) {
        return Fail("config", error);
    }

    MppJpegDecoder decoder;
    if (!decoder.Open(&error)) return Fail("mpp", error);
    Nv12DmaFrame left;
    Nv12DmaFrame right;

    // camera.enable=false skips VI init entirely; enable=true tries the
    // camera and falls back to the configured images when it is unavailable.
    MppViCapture capture;
    bool camera_live = false;
    std::string capture_error;
    if (!config.camera_enable) {
        std::cout << "camera disabled (camera.enable=false); using images\n";
    } else if (capture.Open(config.camera_device, config.camera_width,
                            config.camera_height, config.camera_timeout_ms,
                            config.camera_mipi_lanes, config.camera_mipi_mbps,
                            &capture_error)) {
        camera_live = true;
        std::cout << "input=camera " << config.camera_width << "x"
                << config.camera_height << "\n";
    } else {
        std::cout << "camera unavailable (" << capture_error
                << "); falling back to images\n";
    }

    if (camera_live) {
        if (!capture.CaptureFrames(&left, &right, &error)) {
            capture.Close();
            return Fail("capture", error);
        }
    } else {
        if (!decoder.DecodeFile(config.left_image, &left, &error)) {
            return Fail("left", error);
        }
        if (!decoder.DecodeFile(config.right_image, &right, &error)) {
            decoder.Release(&left);
            return Fail("right", error);
        }
        std::cout << "input=images " << left.width << "x" << left.height << "\n";
    }

    if (left.width != config.camera_width ||
        left.height != config.camera_height ||
        right.width != config.camera_width ||
        right.height != config.camera_height) {
        std::ostringstream expected;
        expected << "expected " << config.camera_width << "x"
                << config.camera_height << " frames, got "
                << left.width << "x" << left.height << " (left) and "
                << right.width << "x" << right.height << " (right)";
        if (camera_live) capture.ReleasePending();
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("input", expected.str());
    }

    if (left.y_data == nullptr || right.y_data == nullptr) {
        if (camera_live) capture.ReleasePending();
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("input", "frames have no CPU-readable Y plane for registration");
    }


    RegistrationResult registration;
    RegistrationOptions options;
    options.work_max_width = config.work_max_width;
    options.fast_threshold = config.fast_threshold;
    options.max_features = config.max_features;
    options.ransac_iterations = config.ransac_iterations;
    options.ransac_threshold_px = config.ransac_threshold_px;
    options.max_reprojection_rmse_px = config.max_reprojection_rmse_px;
    options.registration_mode = config.registration_mode;
    options.registration_file = config.registration_file;
    options.registration_save_to = config.registration_save_to;
    if (!RegisterHomography(
            {left.y_data, left.width, left.height,
            static_cast<int>(left.y_stride)},
            {right.y_data, right.width, right.height,
            static_cast<int>(right.y_stride)},
            options, &registration, &error)) {
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("registration", error);
    }

    Nv12View left_nv12{left.y_data, left.uv_data, left.width, left.height,
                        static_cast<int>(left.y_stride),
                        static_cast<int>(left.uv_stride)};
    Nv12View right_nv12{right.y_data, right.uv_data, right.width, right.height,
                        static_cast<int>(right.y_stride),
                        static_cast<int>(right.uv_stride)};
    const RgbGain gain = EstimateRightRgbGain(
        left_nv12, right_nv12, registration.right_to_left);
    const ExposureModel exposure{{gain.r, gain.g, gain.b},
                                {gain.bias_r, gain.bias_g, gain.bias_b}};

    ProjectedBlendPair projected;
    if (!ProjectPlanarPair(left_nv12, right_nv12, registration, exposure,
                            &projected, &error)) {
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("projection_init", error);
    }

    BlendMaskPyramid masks;
    const int requested_bands = config.num_bands == 0 ? 5 : config.num_bands;
    if (!BuildGraphCutMaskPyramid(projected, requested_bands, &masks, &error)) {
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("graphcut", error);
    }
    const int bands = config.num_bands == 0
        ? ChooseBlendBands(registration.planar_bounds.width,
                            registration.planar_bounds.height,
                            masks.overlap_width)
        : config.num_bands;
    masks.levels.resize(static_cast<size_t>(bands));
    std::cout << "seam_overlap_width=" << masks.overlap_width
                << " blend_bands=" << bands << '\n';

    EglWindow window;
    if (!window.Open(1920, 1080, "Planar Stitcher", config.force_offscreen,
                    &error)) {
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("egl", error);
    }
    std::cout << "render_target=" << (window.is_offscreen() ? "offscreen" : "display")
                << " frames=" << config.frames << '\n';
    PlanarRenderer renderer;
    if (!renderer.Initialize(window, &error) ||
        !renderer.Prepare(registration, masks, &error)) {
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("renderer", error);
    }
    projected = ProjectedBlendPair{};
    masks = BlendMaskPyramid{};

    using Clock = std::chrono::steady_clock;
    uint64_t frames = 0;
    double total_gpu_ms = 0.0;
    const auto run_start = Clock::now();
    auto report_start = Clock::now();
    uint64_t report_frames = 0;
    double report_gpu_ms = 0.0;
    if (config.frames <= 0) {
        std::cout << "frames=0: rendering until Ctrl+C"
                << (window.is_offscreen() ? "" : " (or Esc / window close)")
                << '\n';
    }
    // Mirrors sv_avm: run until the frame budget is reached, a signal arrives,
    // or (on-screen only) the window asks to exit. The last frame is saved after.
    while (g_exit == 0 && (config.frames <= 0 || frames < static_cast<uint64_t>(config.frames))) {
        const auto frame_start = Clock::now();
        // Live camera: pull a fresh pair each frame. Registration, exposure and
        // the seam masks stay fixed from init, matching sv_avm.
        if (camera_live && frames > 0) {
            if (!capture.CaptureFrames(&left, &right, &error)) {
                std::cerr << "capture: " << error << "; stopping\n";
                break;
            }
        }
        if (!renderer.RenderFrame(left, right, exposure, &error)) {
            capture.Close();
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
        if (!window.is_offscreen() && !renderer.Present(&error)) {
            capture.Close();
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
                        << " bands=" << bands << '\n';
            report_start = now;
            report_frames = 0;
            report_gpu_ms = 0.0;
        }
        if (config.sleep_us > 0) {
            usleep(static_cast<useconds_t>(config.sleep_us));
        }
        if (window.PollExitRequested()) break;
    }
    const double run_seconds = std::chrono::duration<double>(
        Clock::now() - run_start).count();

    if (frames == 0) {
        // Interrupted before the first frame: nothing was rendered to save.
        std::cout << "stitch_frames=0 (interrupted before first frame)\n";
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return 0;
    }

    std::cout << std::fixed << std::setprecision(2)
                << "stitch_frames=" << frames
                << " avg_fps=" << (run_seconds > 0.0 ? frames / run_seconds : 0.0)
                << " avg_gpu_ms=" << (total_gpu_ms / frames)
                << " bands=" << bands << '\n';

    std::vector<uint8_t> bgr;
    if (!renderer.Readback(&bgr, &error)) {
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("readback", error);
    }

    const std::filesystem::path output(config.output);
    if (!output.parent_path().empty()) {
        std::filesystem::create_directories(output.parent_path());
    }
    cv::Mat image(renderer.output_height(), renderer.output_width(), CV_8UC3,
                bgr.data());
    if (!cv::imwrite(config.output, image)) {
        capture.Close();
        decoder.Release(&left);
        decoder.Release(&right);
        return Fail("save", config.output);
    }
    std::cout << "saved=" << config.output << '\n';

    capture.Close();
    decoder.Release(&left);
    decoder.Release(&right);
    return 0;
}
