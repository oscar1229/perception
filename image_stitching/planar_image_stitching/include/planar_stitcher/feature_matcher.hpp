#pragma once

#include "planar_stitcher/types.hpp"

#include <cstdint>
#include <string>

namespace planar_stitcher {

struct LumaView {
    const uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
};

struct RegistrationOptions {
    int work_max_width = 960;
    int fast_threshold = 20;
    int max_features = 2000;
    int ransac_iterations = 2000;
    double ransac_threshold_px = 2.0;
    double max_reprojection_rmse_px = 2.0;
    std::string registration_mode = "auto";  // "auto" or "file"
    std::string registration_file;           // read from this when mode="file"
    std::string registration_save_to;        // save to this if non-empty
};

struct RegistrationResult {
    Mat3 right_to_left;
    RectI planar_bounds;
    int matches = 0;
    int inliers = 0;
    double reprojection_rmse = 0.0;
};

struct Nv12View {
    const uint8_t* y_data = nullptr;
    const uint8_t* uv_data = nullptr;
    int width = 0;
    int height = 0;
    int y_stride = 0;
    int uv_stride = 0;
};

struct RgbGain {
    double r = 1.0;
    double g = 1.0;
    double b = 1.0;
    double bias_r = 0.0;
    double bias_g = 0.0;
    double bias_b = 0.0;
};

struct RgbSample {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

bool RegisterHomography(const LumaView& left, const LumaView& right,
                        const RegistrationOptions& options,
                        RegistrationResult* result, std::string* error);

RgbGain EstimateRightRgbGain(const Nv12View& left, const Nv12View& right,
                             const Mat3& right_to_left);
RgbSample ReadNv12Rgb(const Nv12View& view, int x, int y);

}  // namespace planar_stitcher
