#pragma once
#include "planar_stitcher/feature_matcher.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace planar_stitcher {
struct RgbImage { int width=0; int height=0; std::vector<uint8_t> pixels; };
struct ExposureModel { double gain[3]={1.0,1.0,1.0}; double bias[3]={0.0,0.0,0.0}; };
struct ProjectedBlendPair {
    RgbImage left;
    RgbImage right;
    std::vector<uint8_t> left_valid;
    std::vector<uint8_t> right_valid;
};
struct MaskLevel {
    int width=0;
    int height=0;
    std::vector<float> left;
    std::vector<float> right;
};
struct BlendMaskPyramid {
    int overlap_width=0;
    std::vector<MaskLevel> levels;
};
bool EstimateExposure(const RgbImage&, const RgbImage&, int, ExposureModel*, std::string*);
int ChooseBlendBands(int width, int height, int overlap_width);
bool ProjectPlanarPair(const Nv12View& left, const Nv12View& right,
                       const RegistrationResult& registration,
                       const ExposureModel& exposure,
                       ProjectedBlendPair* pair, std::string* error);
bool BuildGraphCutMaskPyramid(const ProjectedBlendPair& pair, int bands,
                              BlendMaskPyramid* pyramid, std::string* error);
bool MultibandBlend(const ProjectedBlendPair&,
                    const BlendMaskPyramid&,
                    std::vector<uint8_t>* output, std::string* error);
}
