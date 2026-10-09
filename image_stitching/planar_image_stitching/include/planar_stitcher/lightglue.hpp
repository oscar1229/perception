/**
 * @file lightglue.h
 * @brief LightGlue feature matcher — ONNX inference wrapper
 */

#ifndef PLANAR_STITCHER_LIGHTGLUE_HPP
#define PLANAR_STITCHER_LIGHTGLUE_HPP

#include <string>
#include <vector>
#include <memory>
#include <onnxruntime_cxx_api.h>

#include "spacemit_ort_env.h"

struct MatchResult {
    std::vector<int> matches;    ///< (M, 3) flattened [batch_idx, kpt0_idx, kpt1_idx]
    std::vector<float> mscores;  ///< (M,) match confidence
    int num_matches = 0;
};

class LightGlue {
public:
    explicit LightGlue(float filter_threshold = 0.1f);

    void load_model(const std::string& model_path,
                    int num_threads = 4,
                    const std::string& provider = "CPUExecutionProvider");

    /// Run ONNX inference. Returns scores (B, N, N) flattened, and B/N.
    std::vector<float> inference(const float* norm_kpts, const float* descriptors,
                                 int B, int N, int& out_B, int& out_N) const;

    MatchResult postprocess(const float* scores, int B, int N) const;

private:
    float filter_threshold_;
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
};

#endif // PLANAR_STITCHER_LIGHTGLUE_HPP
