#include "Resnet18TrafficCamNet.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <memory>
#include <any>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace bsp_dnn
{

int Resnet18TrafficCamNet::preProcess(ObjDetectParams& params, ObjDetectInput& inputData, 
                                      IDnnEngine::dnnInput& outputData)
{
    // 验证输入类型
    if (inputData.handleType != "opencv4")
    {
        std::cerr << "[Resnet18TrafficCamNet] Error: Only opencv4 handle type is supported" << std::endl;
        return -1;
    }

    // 获取输入图像
    auto imagePtr = std::any_cast<std::shared_ptr<cv::Mat>>(inputData.imageHandle);
    if (!imagePtr || imagePtr->empty())
    {
        std::cerr << "[Resnet18TrafficCamNet] Error: Input image is nullptr or empty" << std::endl;
        return -1;
    }

    cv::Mat origImage = *imagePtr;

    // 步骤1: BGR转RGB
    cv::Mat rgbImage;
    cv::cvtColor(origImage, rgbImage, cv::COLOR_BGR2RGB);

    // 步骤2: 计算缩放比例（保持宽高比的letterbox）
    float scaleWidth = static_cast<float>(params.model_input_width) / origImage.cols;
    float scaleHeight = static_cast<float>(params.model_input_height) / origImage.rows;

    // 使用最小缩放比例以保持宽高比
    float minScale = std::min(scaleWidth, scaleHeight);
    params.scale_width = minScale;
    params.scale_height = minScale;

    // 计算缩放后的尺寸
    int newWidth = static_cast<int>(origImage.cols * minScale);
    int newHeight = static_cast<int>(origImage.rows * minScale);

    // Resize图像
    cv::Mat resizedImage;
    cv::resize(rgbImage, resizedImage, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_LINEAR);

    // 步骤3: 添加letterbox padding (灰色填充)
    int padLeft = (params.model_input_width - newWidth) / 2;
    int padTop = (params.model_input_height - newHeight) / 2;
    int padRight = params.model_input_width - newWidth - padLeft;
    int padBottom = params.model_input_height - newHeight - padTop;

    params.pads.left = padLeft;
    params.pads.top = padTop;
    params.pads.right = padRight;
    params.pads.bottom = padBottom;

    cv::Mat paddedImage;
    cv::copyMakeBorder(resizedImage, paddedImage, padTop, padBottom, padLeft, padRight,
                      cv::BORDER_CONSTANT, cv::Scalar(128, 128, 128));

    // 步骤4: 归一化并转换为CHW planar格式 (float32)
    // ResNet18 TrafficCamNet使用 scale_factor = 1/255 = 0.00392156862745098
    const float scaleFactor = 0.00392156862745098f;

    // 分配输出缓冲区 (CHW格式的float32数据)
    size_t totalSize = params.model_input_width * params.model_input_height * params.model_input_channel;
    size_t bytesPerChannel = params.model_input_width * params.model_input_height;

    outputData.index = 0;
    outputData.shape.width = params.model_input_width;
    outputData.shape.height = params.model_input_height;
    outputData.shape.channel = params.model_input_channel;
    outputData.size = totalSize * sizeof(float);
    outputData.dataType = "float32";

    if (outputData.buf.size() != outputData.size)
    {
        outputData.buf.resize(outputData.size);
    }

    // 转换为CHW planar格式并归一化
    float* floatPtr = reinterpret_cast<float*>(outputData.buf.data());

    for (int c = 0; c < 3; c++)
    {
        for (int h = 0; h < params.model_input_height; h++)
        {
            for (int w = 0; w < params.model_input_width; w++)
            {
                int dstIndex = c * bytesPerChannel + h * params.model_input_width + w;
                uint8_t pixelValue = paddedImage.at<cv::Vec3b>(h, w)[c];
                floatPtr[dstIndex] = pixelValue * scaleFactor;
            }
        }
    }

    return 0;
}

int Resnet18TrafficCamNet::loadLabels(const std::string& labelPath)
{
    if (m_labelsLoaded)
    {
        return 0;  // 已经加载过了
    }
    
    std::ifstream file(labelPath);
    if (!file.is_open())
    {
        std::cerr << "[Resnet18TrafficCamNet] Error: Cannot open label file: " << labelPath << std::endl;
        return -1;
    }
    
    m_labels.clear();
    std::string line;
    while (std::getline(file, line))
    {
        // 去除首尾空白字符
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (!line.empty())
        {
            m_labels.push_back(line);
        }
    }
    
    file.close();
    m_labelsLoaded = true;
    
    std::cout << "[Resnet18TrafficCamNet] Loaded " << m_labels.size() << " labels from " << labelPath << std::endl;
    return 0;
}

float Resnet18TrafficCamNet::calculateIoU(const ObjDetectOutputBox& box1, const ObjDetectOutputBox& box2)
{
    // 计算交集区域
    int x1 = std::max(box1.bbox.left, box2.bbox.left);
    int y1 = std::max(box1.bbox.top, box2.bbox.top);
    int x2 = std::min(box1.bbox.right, box2.bbox.right);
    int y2 = std::min(box1.bbox.bottom, box2.bbox.bottom);
    
    if (x2 < x1 || y2 < y1)
    {
        return 0.0f;  // 没有交集
    }
    
    float intersectionArea = (x2 - x1) * (y2 - y1);
    
    // 计算并集区域
    float box1Area = (box1.bbox.right - box1.bbox.left) * (box1.bbox.bottom - box1.bbox.top);
    float box2Area = (box2.bbox.right - box2.bbox.left) * (box2.bbox.bottom - box2.bbox.top);
    float unionArea = box1Area + box2Area - intersectionArea;
    
    if (unionArea <= 0.0f)
    {
        return 0.0f;
    }
    
    return intersectionArea / unionArea;
}

std::vector<ObjDetectOutputBox> Resnet18TrafficCamNet::applyNMS(std::vector<ObjDetectOutputBox>& boxes,
                                                                 float nmsThreshold)
{
    if (boxes.empty())
    {
        return {};
    }

    // 按置信度降序排序
    std::sort(boxes.begin(), boxes.end(),
              [](const ObjDetectOutputBox& a, const ObjDetectOutputBox& b) {
                  return a.score > b.score;
              });

    std::vector<ObjDetectOutputBox> result;
    std::vector<bool> suppressed(boxes.size(), false);

    for (size_t i = 0; i < boxes.size(); i++)
    {
        if (suppressed[i])
        {
            continue;
        }

        result.push_back(boxes[i]);

        // 抑制与当前框IoU大于阈值的其他框
        for (size_t j = i + 1; j < boxes.size(); j++)
        {
            if (suppressed[j])
            {
                continue;
            }

            // 只对同一类别应用NMS
            if (boxes[i].label == boxes[j].label)
            {
                float iou = calculateIoU(boxes[i], boxes[j]);
                if (iou > nmsThreshold)
                {
                    suppressed[j] = true;
                }
            }
        }
    }

    return result;
}

void Resnet18TrafficCamNet::parseDetectNetOutput(const float* coverageBuffer, const float* bboxBuffer,
                                                 int gridW, int gridH, int numClasses,
                                                 const ObjDetectParams& params,
                                                 std::vector<ObjDetectOutputBox>& outputData)
{
    const int gridSize = gridW * gridH;
    const int networkWidth = params.model_input_width;
    const int networkHeight = params.model_input_height;

    // 计算stride
    int strideX = (networkWidth + gridW - 1) / gridW;
    int strideY = (networkHeight + gridH - 1) / gridH;

    // 计算网格中心点
    std::vector<float> gcCentersX(gridW);
    std::vector<float> gcCentersY(gridH);

    for (int i = 0; i < gridW; i++)
    {
        gcCentersX[i] = (i * strideX + 0.5f) / BBOX_NORM[0];
    }
    for (int i = 0; i < gridH; i++)
    {
        gcCentersY[i] = (i * strideY + 0.5f) / BBOX_NORM[1];
    }

    // 遍历每个类别
    for (int classIdx = 0; classIdx < numClasses; classIdx++)
    {
        // 获取当前类别的bbox指针
        const float* outputX1 = bboxBuffer + classIdx * 4 * gridSize;
        const float* outputY1 = outputX1 + gridSize;
        const float* outputX2 = outputY1 + gridSize;
        const float* outputY2 = outputX2 + gridSize;

        // 遍历网格
        for (int h = 0; h < gridH; h++)
        {
            for (int w = 0; w < gridW; w++)
            {
                int gridIdx = h * gridW + w;
                float confidence = coverageBuffer[classIdx * gridSize + gridIdx];

                // 应用置信度阈值
                if (confidence < params.conf_threshold)
                {
                    continue;
                }

                // 解码边界框坐标（DetectNet格式）
                float rectX1 = (outputX1[gridIdx] - gcCentersX[w]) * (-BBOX_NORM[0]);
                float rectY1 = (outputY1[gridIdx] - gcCentersY[h]) * (-BBOX_NORM[1]);
                float rectX2 = (outputX2[gridIdx] + gcCentersX[w]) * BBOX_NORM[0];
                float rectY2 = (outputY2[gridIdx] + gcCentersY[h]) * BBOX_NORM[1];

                // 裁剪到图像边界
                rectX1 = std::max(0.0f, std::min(rectX1, static_cast<float>(networkWidth - 1)));
                rectY1 = std::max(0.0f, std::min(rectY1, static_cast<float>(networkHeight - 1)));
                rectX2 = std::max(0.0f, std::min(rectX2, static_cast<float>(networkWidth - 1)));
                rectY2 = std::max(0.0f, std::min(rectY2, static_cast<float>(networkHeight - 1)));

                // 检查有效性
                if (rectX2 <= rectX1 || rectY2 <= rectY1)
                {
                    continue;
                }

                // 将坐标从模型输入空间转换回原始图像空间
                // 考虑letterbox padding
                float x1 = (rectX1 - params.pads.left) / params.scale_width;
                float y1 = (rectY1 - params.pads.top) / params.scale_height;
                float x2 = (rectX2 - params.pads.left) / params.scale_width;
                float y2 = (rectY2 - params.pads.top) / params.scale_height;

                // 创建检测框
                ObjDetectOutputBox box;
                box.bbox.left = static_cast<int>(x1);
                box.bbox.top = static_cast<int>(y1);
                box.bbox.right = static_cast<int>(x2);
                box.bbox.bottom = static_cast<int>(y2);
                box.score = confidence;

                // 设置标签
                if (classIdx < static_cast<int>(m_labels.size()))
                {
                    box.label = m_labels[classIdx];
                }
                else
                {
                    box.label = "Class_" + std::to_string(classIdx);
                }

                outputData.push_back(box);
            }
        }
    }
}

int Resnet18TrafficCamNet::postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
                                       std::vector<IDnnEngine::dnnOutput>& inputData, 
                                       std::vector<ObjDetectOutputBox>& outputData)
{
    // 验证输入
    if (inputData.size() != EXPECTED_OUTPUTS)
    {
        std::cerr << "[Resnet18TrafficCamNet] Error: Expected " << EXPECTED_OUTPUTS 
                  << " output tensors, got " << inputData.size() << std::endl;
        return -1;
    }

    // 加载标签
    if (!m_labelsLoaded && !labelTextPath.empty())
    {
        if (loadLabels(labelTextPath) != 0)
        {
            std::cerr << "[Resnet18TrafficCamNet] Warning: Failed to load labels" << std::endl;
        }
    }

    // 查找coverage和bbox层
    // ResNet18 TrafficCamNet输出：
    // - 输出0: coverage层 [num_classes, grid_h, grid_w]
    // - 输出1: bbox层 [num_classes*4, grid_h, grid_w]
    const float* coverageBuffer = nullptr;
    const float* bboxBuffer = nullptr;
    size_t coverageSize = 0;
    size_t bboxSize = 0;

    // 假设输出顺序：coverage在前，bbox在后
    // 或者根据大小判断（bbox层应该是coverage层的4倍大）
    if (inputData[0].size < inputData[1].size)
    {
        // inputData[0]是coverage，inputData[1]是bbox
        coverageBuffer = reinterpret_cast<const float*>(inputData[0].buf);
        coverageSize = inputData[0].size / sizeof(float);
        bboxBuffer = reinterpret_cast<const float*>(inputData[1].buf);
        bboxSize = inputData[1].size / sizeof(float);
    }
    else
    {
        // inputData[1]是coverage，inputData[0]是bbox
        coverageBuffer = reinterpret_cast<const float*>(inputData[1].buf);
        coverageSize = inputData[1].size / sizeof(float);
        bboxBuffer = reinterpret_cast<const float*>(inputData[0].buf);
        bboxSize = inputData[0].size / sizeof(float);
    }

    // 验证数据有效性
    if (!coverageBuffer || !bboxBuffer)
    {
        std::cerr << "[Resnet18TrafficCamNet] Error: Invalid output buffers" << std::endl;
        return -1;
    }

    // 推断网格尺寸和类别数
    // TrafficCamNet默认有4个类别
    const int numClasses = 4;  // Vehicle, TwoWheeler, Person, RoadSign

    // 计算网格尺寸
    int gridSize = coverageSize / numClasses;
    int gridW = 60;  // 典型值，可能需要根据实际模型调整
    int gridH = 34;  // 典型值，可能需要根据实际模型调整

    // 验证尺寸
    if (gridW * gridH != gridSize)
    {
        // 尝试计算实际网格尺寸
        gridW = static_cast<int>(std::sqrt(gridSize));
        gridH = gridSize / gridW;

        std::cout << "[Resnet18TrafficCamNet] Computed grid size: " << gridW << "x" << gridH << std::endl;
    }
    
    // 解析DetectNet输出
    std::vector<ObjDetectOutputBox> detections;
    parseDetectNetOutput(coverageBuffer, bboxBuffer, gridW, gridH, numClasses, params, detections);
    
    std::cout << "[Resnet18TrafficCamNet] Found " << detections.size() << " raw detections" << std::endl;
    
    // 应用NMS
    outputData = applyNMS(detections, params.nms_threshold);
    
    std::cout << "[Resnet18TrafficCamNet] After NMS: " << outputData.size() << " detections" << std::endl;
    
    return 0;
}

} // namespace bsp_dnn

