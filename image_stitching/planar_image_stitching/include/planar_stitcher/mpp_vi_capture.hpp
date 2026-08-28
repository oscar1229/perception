#pragma once

#include "planar_stitcher/mpp_jpeg_decoder.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace planar_stitcher {

// Two-channel MPP VI capture producing NV12 DMA-BUF frames.
//
// Format note: sv_avm captures UYVY because its sensor path delivers that, but
// this pipeline is NV12 end to end (decoder, EGLImage import, shaders, tests).
// VI is configured for MPP_PIXEL_FORMAT_NV12 so camera frames and JPEG frames
// share one format and the render path needs no conversion.
class MppViCapture {
public:
    static constexpr int kChannels = 2;

    MppViCapture() = default;
    ~MppViCapture();
    MppViCapture(const MppViCapture&) = delete;
    MppViCapture& operator=(const MppViCapture&) = delete;

    // Brings up SYS/VB/VI and enables both channels. Returns false when no
    // camera is usable, leaving the caller to fall back to the JPEG inputs.
    bool Open(int device, int width, int height, int timeout_ms,
              int mipi_lanes, int mbps, std::string* error);

    // Fills left/right with the newest frame from each channel. Frames stay
    // owned by VI until ReleasePending(); call it once per rendered frame.
    bool CaptureFrames(Nv12DmaFrame* left, Nv12DmaFrame* right,
                       std::string* error);

    // Returns the frames held since the last CaptureFrames back to VI.
    void ReleasePending() noexcept;

    void Close() noexcept;

    bool is_open() const { return vi_initialized_; }

private:
    struct ChannelState {
        bool configured = false;
        bool enabled = false;
        bool pending = false;
    };

    // Opaque handle to the VideoFrameInfo pair held between CaptureFrames and
    // ReleasePending; defined in the .cpp so the MPP headers stay out of here.
    struct PendingFrames;

    bool sys_initialized_ = false;
    bool vb_initialized_ = false;
    bool vi_initialized_ = false;
    bool device_enabled_ = false;
    int device_ = 0;
    int width_ = 0;
    int height_ = 0;
    int timeout_ms_ = 33;
    std::array<ChannelState, kChannels> channels_{};
    PendingFrames* pending_ = nullptr;
};

}  // namespace planar_stitcher
