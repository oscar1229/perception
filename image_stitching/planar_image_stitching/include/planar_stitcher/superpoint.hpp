/**
 * @file superpoint.h
 * @brief SuperPoint feature extractor — ONNX inference wrapper
 */

#ifndef PLANAR_STITCHER_SUPERPOINT_HPP
#define PLANAR_STITCHER_SUPERPOINT_HPP

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

#include "spacemit_ort_env.h"

struct PreprocessResult {
    std::vector<float> tensor;  ///< (1, 1, H, W) flattened
    cv::Mat resized_bgr;        ///< BGR resized image
    float scale;
    int pad_left;
    int pad_top;
};

struct SPPostprocessResult {
    std::vector<int> keypoints;      ///< (B, N, 2) flattened [x, y]
    std::vector<float> scores;       ///< (B, N) flattened
    std::vector<float> descriptors;  ///< (B, N, 256) flattened
};

class SuperPoint {
public:
    SuperPoint(int num_keypoints, int nms_radius, int remove_borders);

    void load_model(const std::string& model_path,
                    int num_threads = 4,
                    const std::string& provider = "CPUExecutionProvider");

    PreprocessResult preprocess(const cv::Mat& image_bgr) const;

    /// Run ONNX inference. input_data must have batch * 1 * input_h * input_w elements.
    /// Returns {scores_flat, descriptor_map_flat}.
    std::pair<std::vector<float>, std::vector<float>>
    inference(const float* input_data, int batch) const;

    SPPostprocessResult postprocess(const float* scores_data,
                                    const float* desc_map_data,
                                    int batch) const;

    /// Pixel coords -> [-1, 1] normalized. kpts has B*N*2 elements.
    std::vector<float> normalize_keypoints(const int* kpts, int B, int N) const;

    /// Model coords -> original image coords. kpts has N*2 elements.
    std::vector<double> keypoints_to_original(const int* kpts, int N,
                                              float scale, int pad_left, int pad_top,
                                              int orig_w = 0, int orig_h = 0) const;

    int input_h() const { return input_h_; }
    int input_w() const { return input_w_; }

    /// Set current detection result as reference (first frame)
    void set_reference(const cv::Mat& image, const PreprocessResult& pre,
                       const std::vector<int>& keypoints,
                       const std::vector<float>& descriptors, int num_keypoints);

    /// Update reference from current frame (call after matching)
    void update_reference(const cv::Mat& image, const PreprocessResult& pre,
                          const std::vector<int>& keypoints,
                          const std::vector<float>& descriptors, int num_keypoints);

    bool has_reference() const { return has_ref_; }
    const cv::Mat& ref_image() const { return ref_image_; }
    const cv::Mat& ref_resized_bgr() const { return ref_resized_bgr_; }
    const PreprocessResult& ref_preprocess() const { return ref_pre_; }
    const std::vector<int>& ref_keypoints() const { return ref_keypoints_; }
    const std::vector<float>& ref_descriptors() const { return ref_descriptors_; }
    int ref_num_keypoints() const { return ref_N_; }

private:
    void simple_nms(float* scores, int H, int W) const;
    void sample_descriptors(const float* desc_map, int C, int dH, int dW,
                            const int* kpts, int N, int H, int W,
                            float* out_desc) const;

    int num_keypoints_;
    int nms_radius_;
    int remove_borders_;
    int input_h_ = 0;
    int input_w_ = 0;

    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;

    // Reference frame state
    cv::Mat ref_image_;
    cv::Mat ref_resized_bgr_;
    PreprocessResult ref_pre_;
    std::vector<int> ref_keypoints_;
    std::vector<float> ref_descriptors_;
    int ref_N_ = 0;
    bool has_ref_ = false;
};

#endif // PLANAR_STITCHER_SUPERPOINT_HPP
