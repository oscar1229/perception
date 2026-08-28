#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace planar_stitcher {

struct Nv12DmaFrame {
    int width = 0;
    int height = 0;
    int dma_fd = -1;
    int channel = -1;
    std::uint32_t y_stride = 0;
    std::uint32_t uv_stride = 0;
    std::uint32_t uv_offset = 0;
    const std::uint8_t* y_data = nullptr;
    const std::uint8_t* uv_data = nullptr;
    unsigned long buffer_id = 0;
};

class MppJpegDecoder {
public:
    MppJpegDecoder() = default;
    ~MppJpegDecoder();

    MppJpegDecoder(const MppJpegDecoder&) = delete;
    MppJpegDecoder& operator=(const MppJpegDecoder&) = delete;

    bool Open(std::string* error);
    bool DecodeFile(const std::string& path, Nv12DmaFrame* frame,
                    std::string* error);
    void Release(Nv12DmaFrame* frame) noexcept;
    void Close() noexcept;

private:
    static constexpr int kMaxChannels = 64;
    struct ChannelState {
        bool created = false;
        bool enabled = false;
        unsigned long buffer_id = 0;
    };

    void DestroyChannel(int channel) noexcept;
    int AllocateChannel() noexcept;

    int next_channel_ = 0;
    std::array<ChannelState, kMaxChannels> channels_{};
    bool vdec_initialized_ = false;
    bool vb_initialized_ = false;
    bool sys_initialized_ = false;
};

}  // namespace planar_stitcher
