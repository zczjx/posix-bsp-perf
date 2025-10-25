#ifndef RESNET18_TRAFFICCAMNET_HPP
#define RESNET18_TRAFFICCAMNET_HPP

#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>
#include <vector>
#include <string>

namespace bsp_dnn
{

/**
 * @brief Plugin for ResNet18 TrafficCamNet object detection model
 *
 * This plugin implements preprocessing and postprocessing for NVIDIA's
 * ResNet18-based TrafficCamNet model, which detects:
 * - Vehicle (class 0)
 * - TwoWheeler (class 1)
 * - Person (class 2)
 * - RoadSign (class 3)
 *
 * Model output format (DetectNet architecture):
 * - Coverage layer: [num_classes, grid_h, grid_w] - confidence scores
 * - BBox layer: [num_classes*4, grid_h, grid_w] - bounding box coordinates (x1,y1,x2,y2)
 */
class Resnet18TrafficCamNet : public IDnnObjDetectorPlugin
{
public:
    Resnet18TrafficCamNet() = default;
    ~Resnet18TrafficCamNet() = default;
    /**
     * @brief Preprocess input image for ResNet18 TrafficCamNet
     *
     * Preprocessing steps:
     * 1. Convert BGR to RGB
     * 2. Resize to model input size (with letterbox padding)
     * 3. Normalize pixel values (scale by 1/255)
     * 4. Convert to CHW planar format (NCHW)
     * 
     * @param params Detection parameters including model input shape
     * @param inputData Input image data (opencv4 format)
     * @param outputData Output preprocessed data ready for inference
     * @return 0 on success, non-zero on failure
     */
    int preProcess(ObjDetectParams& params, ObjDetectInput& inputData,
                   IDnnEngine::dnnInput& outputData) override;

    /**
     * @brief Postprocess model outputs to extract detected objects
     * 
     * Parses DetectNet-style outputs:
     * - Coverage layer for confidence scores
     * - BBox layer for bounding box coordinates
     * 
     * Applies:
     * - Confidence threshold filtering
     * - NMS (Non-Maximum Suppression)
     * - Coordinate denormalization
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
    std::vector<std::string> m_labels;
    bool m_labelsLoaded{false};

    // DetectNet specific parameters
    static constexpr int EXPECTED_OUTPUTS = 2;  // coverage + bbox layers
    static constexpr float BBOX_NORM[2] = {35.0f, 35.0f};  // DetectNet normalization factors
};

} // namespace bsp_dnn

// Export plugin creation functions
CREATE_PLUGIN_INSTANCE(bsp_dnn::Resnet18TrafficCamNet)

#endif // RESNET18_TRAFFICCAMNET_HPP

