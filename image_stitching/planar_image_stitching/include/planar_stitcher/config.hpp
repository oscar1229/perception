#pragma once

#include <string>

namespace planar_stitcher {

struct Config {
    // Input images
    std::string left_image;
    std::string right_image;

    // Output
    std::string output;

    // Camera settings (auto-detect and fallback to images)
    int camera_width = 1920;
    int camera_height = 1080;
    int camera_device = 0;
    int camera_timeout_ms = 33;
    int camera_mipi_lanes = 4;
    int camera_mipi_mbps = 800;

    // Registration
    std::string registration_mode = "auto";  // "auto" or "file"
    std::string registration_file;             // read from this when mode="file"
    std::string registration_save_to;          // save to this if non-empty

    // Feature detection
    int work_max_width = 960;
    int fast_threshold = 20;
    int max_features = 2000;

    // RANSAC
    int ransac_iterations = 2000;
    double ransac_threshold_px = 2.0;
    double max_reprojection_rmse_px = 2.0;

    // Blending
    int num_bands = 5;

    // Runtime
    int frames = 1;
    int sleep_us = 0;
    bool force_offscreen = false;
};

bool ParseConfigFile(const std::string& path, Config* config, std::string* error);
bool ParseConfigText(const std::string& text, Config* config, std::string* error);

}
