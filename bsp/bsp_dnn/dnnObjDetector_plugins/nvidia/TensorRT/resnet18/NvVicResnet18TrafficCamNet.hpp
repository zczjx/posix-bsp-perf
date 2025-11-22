#ifndef NVVIC_RESNET18_TRAFFICCAMNET_HPP
#define NVVIC_RESNET18_TRAFFICCAMNET_HPP

#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>
#include <bsp_g2d/IGraphics2D.hpp>
#include <vector>
#include <string>
#include <memory>


namespace bsp_dnn
{
using namespace bsp_g2d;

/**
 * @brief NvVic version of ResNet18 TrafficCamNet plugin
 * 
 * This plugin uses NVIDIA VIC (Video Image Compositor) for hardware-accelerated
 * preprocessing of DecodeOutFrame input data.
 * 
 * Supports DecodeOutFrame input type (YUV420SP from decoder)
 * Uses nvvic for YUV to RGB conversion and resizing
 * 
 * Model output format (DetectNet architecture):
 * - Coverage layer: [num_classes, grid_h, grid_w] - confidence scores
 * - BBox layer: [num_classes*4, grid_h, grid_w] - bounding box coordinates (x1,y1,x2,y2)
 * 
 * Detects:
 * - Vehicle (class 0)
 * - TwoWheeler (class 1)
 * - Person (class 2)
 * - RoadSign (class 3)
 */
class NvVicResnet18TrafficCamNet : public IDnnObjDetectorPlugin
{
public:
    NvVicResnet18TrafficCamNet():
        m_g2d{IGraphics2D::create("nvvic")},
        IDnnObjDetectorPlugin()
    {}
    
    ~NvVicResnet18TrafficCamNet() = default;

    /**
     * @brief Preprocess DecodeOutFrame input for ResNet18 TrafficCamNet
     *
     * Preprocessing steps:
     * 1. Use nvvic to convert YUV420SP to RGBA8888 (NvVic format requirement)
     * 2. Resize to model input size (with letterbox padding if needed)
     * 3. Normalize pixel values (scale by 1/255) and extract RGB channels (drop alpha)
     * 4. Convert to CHW planar format (NCHW) for TensorRT inference
     * 
     * TensorRT ResNet18 TrafficCamNet input requirement:
     * - Format: RGB float32 (3 channels)
     * - Layout: NCHW (batch, channel, height, width)
     * - Normalization: [0, 1] range (pixel / 255.0)
     * 
     * @param params Detection parameters including model input shape
     * @param inputData Input frame data (DecodeOutFrame format)
     * @param outputData Output preprocessed data ready for inference
     * @return 0 on success, non-zero on failure
     */
    int preProcess(ObjDetectParams& params, ObjDetectInput& inputData,
                   IDnnEngine::dnnInput& outputData) override;

    /**
     * @brief Postprocess model outputs to extract detected objects
     * 
     * Same as original Resnet18TrafficCamNet implementation
     * 
     * @param labelTextPath Path to label file
     * @param params Detection parameters (thresholds, scales, etc.)
     * @param inputData Model output tensors (coverage + bbox)
     * @param outputData Detected objects with bounding boxes
     * @return 0 on success, non-zero on failure
     */
    int postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
                   std::vector<IDnnEngine::dnnOutput>& inputData, 
                   std::vector<ObjDetectOutputBox>& outputData) override;

private:
    /**
     * @brief Load class labels from file
     * @param labelPath Path to label text file
     * @return 0 on success, non-zero on failure
     */
    int loadLabels(const std::string& labelPath);

    /**
     * @brief Parse DetectNet bounding box outputs
     * @param coverageBuffer Coverage layer data
     * @param bboxBuffer BBox layer data
     * @param gridW Grid width
     * @param gridH Grid height
     * @param numClasses Number of classes
     * @param params Detection parameters
     * @param outputData Detected objects output
     */
    void parseDetectNetOutput(const float* coverageBuffer, const float* bboxBuffer,
                             int gridW, int gridH, int numClasses,
                             const ObjDetectParams& params,
                             std::vector<ObjDetectOutputBox>& outputData);

    /**
     * @brief Apply Non-Maximum Suppression to remove duplicate detections
     * @param boxes Input bounding boxes
     * @param nmsThreshold IoU threshold for NMS
     * @return Filtered boxes after NMS
     */
    std::vector<ObjDetectOutputBox> applyNMS(std::vector<ObjDetectOutputBox>& boxes,
                                              float nmsThreshold);

    /**
     * @brief Calculate IoU (Intersection over Union) between two boxes
     * @param box1 First bounding box
     * @param box2 Second bounding box
     * @return IoU value [0, 1]
     */
    float calculateIoU(const ObjDetectOutputBox& box1, const ObjDetectOutputBox& box2);

private:
    std::unique_ptr<IGraphics2D> m_g2d{nullptr};
    std::vector<uint8_t> m_rgb_buffer{};  // RGBA buffer for intermediate processing (NvVic output)
    std::vector<std::string> m_labels;
    bool m_labelsLoaded{false};

    // DetectNet specific parameters
    static constexpr int EXPECTED_OUTPUTS = 2;  // coverage + bbox layers
    static constexpr float BBOX_NORM[2] = {35.0f, 35.0f};  // DetectNet normalization factors
};

} // namespace bsp_dnn

// Export plugin creation functions
CREATE_PLUGIN_INSTANCE(bsp_dnn::NvVicResnet18TrafficCamNet)

#endif // NVVIC_RESNET18_TRAFFICCAMNET_HPP

