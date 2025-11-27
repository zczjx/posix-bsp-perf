#include "NvVicResnet18TrafficCamNet.hpp"
#include <bsp_codec/IDecoder.hpp>
#include <memory>
#include <any>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace bsp_dnn
{
using namespace bsp_codec;

int NvVicResnet18TrafficCamNet::preProcess(ObjDetectParams& params, ObjDetectInput& inputData,
                                           IDnnEngine::dnnInput& outputData)
{
    // 验证输入类型
    if (inputData.handleType != "DecodeOutFrame")
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] Error: Only DecodeOutFrame handle type is supported" << std::endl;
        return -1;
    }

    // 获取输入的 YUV420 帧
    auto yuv420_frame = std::any_cast<std::shared_ptr<DecodeOutFrame>>(inputData.imageHandle);
    if (yuv420_frame == nullptr)
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] Error: Input frame is nullptr" << std::endl;
        return -1;
    }

    if (m_g2d == nullptr)
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] Error: G2D engine is nullptr" << std::endl;
        return -1;
    }

    // 计算缩放参数（直接拉伸，不保持宽高比）
    params.scale_width = static_cast<float>(params.model_input_width) / yuv420_frame->width;
    params.scale_height = static_cast<float>(params.model_input_height) / yuv420_frame->height;
    params.pads.left = 0;
    params.pads.top = 0;
    params.pads.right = 0;
    params.pads.bottom = 0;

    std::cout << "[NvVicResnet18TrafficCamNet] Input: " << yuv420_frame->width << "x" << yuv420_frame->height 
              << " → Model: " << params.model_input_width << "x" << params.model_input_height 
              << ", Scale: " << params.scale_width << "x" << params.scale_height << std::endl;

    // ========== 使用新的 IGraphics2D API ==========
    
    // 修复 stride 为 0 的问题
    size_t input_width_stride = yuv420_frame->width_stride > 0 ? 
                                static_cast<size_t>(yuv420_frame->width_stride) : 
                                static_cast<size_t>(yuv420_frame->width);
    size_t input_height_stride = yuv420_frame->height_stride > 0 ? 
                                 static_cast<size_t>(yuv420_frame->height_stride) : 
                                 static_cast<size_t>(yuv420_frame->height);

    // 步骤1: 创建输入 YUV420 帧的 Mapped 缓冲区
    IGraphics2D::G2DBufferParams yuv420_params;
    yuv420_params.host_ptr = yuv420_frame->virt_addr;
    yuv420_params.buffer_size = yuv420_frame->valid_data_size;
    yuv420_params.width = static_cast<size_t>(yuv420_frame->width);
    yuv420_params.height = static_cast<size_t>(yuv420_frame->height);
    yuv420_params.width_stride = input_width_stride;
    yuv420_params.height_stride = input_height_stride;
    yuv420_params.format = yuv420_frame->format;
    
    auto yuv420_g2d_buf = m_g2d->createBuffer(IGraphics2D::BufferType::Mapped, yuv420_params);
    if (!yuv420_g2d_buf)
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] Error: Failed to create YUV420 G2D buffer" << std::endl;
        return -1;
    }

    // 步骤2: 创建输出 RGBA8888 帧的 Mapped 缓冲区
    size_t rgba_buffer_size = params.model_input_width * params.model_input_height * 4;
    if (m_rgb_buffer.size() != rgba_buffer_size)
    {
        m_rgb_buffer.resize(rgba_buffer_size);
    }

    IGraphics2D::G2DBufferParams rgba_params;
    rgba_params.host_ptr = m_rgb_buffer.data();
    rgba_params.buffer_size = rgba_buffer_size;
    rgba_params.width = static_cast<size_t>(params.model_input_width);
    rgba_params.height = static_cast<size_t>(params.model_input_height);
    rgba_params.width_stride = static_cast<size_t>(params.model_input_width);
    rgba_params.height_stride = static_cast<size_t>(params.model_input_height);
    rgba_params.format = "RGBA8888";
    
    auto rgba_g2d_buf = m_g2d->createBuffer(IGraphics2D::BufferType::Mapped, rgba_params);
    if (!rgba_g2d_buf)
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] Error: Failed to create RGBA G2D buffer" << std::endl;
        m_g2d->releaseBuffer(yuv420_g2d_buf);
        return -1;
    }

    // 步骤3: 硬件加速：YUV → RGBA + Resize
    int ret = m_g2d->imageResize(yuv420_g2d_buf, rgba_g2d_buf);
    if (ret != 0)
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] Error: imageResize failed with code " << ret << std::endl;
        m_g2d->releaseBuffer(rgba_g2d_buf);
        m_g2d->releaseBuffer(yuv420_g2d_buf);
        return ret;
    }

    // 步骤4: 同步 Device → CPU（VIC 需要显式同步）
    ret = m_g2d->syncBuffer(rgba_g2d_buf, IGraphics2D::SyncDirection::DeviceToCpu);
    if (ret != 0)
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] Warning: syncBuffer returned " << ret << ", continuing anyway" << std::endl;
    }

    // 释放 G2D 缓冲区
    m_g2d->releaseBuffer(rgba_g2d_buf);
    m_g2d->releaseBuffer(yuv420_g2d_buf);

    std::cout << "[NvVicResnet18TrafficCamNet] Hardware conversion completed" << std::endl;

    // 步骤5: 归一化并转换为 CHW planar 格式 (float32)
    const float scaleFactor = 0.00392156862745098f; // 1/255

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

    // 转换为 CHW planar 格式并归一化
    // 注意：输入是 RGBA 格式（4通道），需要提取 RGB 三个通道
    float* floatPtr = reinterpret_cast<float*>(outputData.buf.data());
    const uint8_t* rgbaPtr = m_rgb_buffer.data();

    // 使用 BGR 顺序（NVIDIA VIC 输出 BGRA）
    const bool USE_BGR_ORDER = true;
    
    if (USE_BGR_ORDER)
    {
        // BGR 顺序：通道映射 [2,1,0] → [R,G,B]
        for (int c = 0; c < 3; c++)
        {
            int srcChannel = 2 - c;  // 反转：B=2, G=1, R=0
            for (int h = 0; h < params.model_input_height; h++)
            {
                for (int w = 0; w < params.model_input_width; w++)
                {
                    int srcIndex = (h * params.model_input_width + w) * 4 + srcChannel;
                    int dstIndex = c * bytesPerChannel + h * params.model_input_width + w;
                    floatPtr[dstIndex] = rgbaPtr[srcIndex] * scaleFactor;
                }
            }
        }
    }
    else
    {
        // RGB 顺序（标准）
        for (int c = 0; c < 3; c++)
        {
            for (int h = 0; h < params.model_input_height; h++)
            {
                for (int w = 0; w < params.model_input_width; w++)
                {
                    int srcIndex = (h * params.model_input_width + w) * 4 + c;
                    int dstIndex = c * bytesPerChannel + h * params.model_input_width + w;
                    floatPtr[dstIndex] = rgbaPtr[srcIndex] * scaleFactor;
                }
            }
        }
    }

    std::cout << "[NvVicResnet18TrafficCamNet] Preprocessing completed successfully" << std::endl;

    return 0;
}

int NvVicResnet18TrafficCamNet::loadLabels(const std::string& labelPath)
{
    if (m_labelsLoaded)
    {
        return 0;  // 已经加载过了
    }
    
    std::ifstream file(labelPath);
    if (!file.is_open())
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] Error: Cannot open label file: " << labelPath << std::endl;
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
    
    std::cout << "[NvVicResnet18TrafficCamNet] Loaded " << m_labels.size() << " labels from " << labelPath << std::endl;
    return 0;
}

float NvVicResnet18TrafficCamNet::calculateIoU(const ObjDetectOutputBox& box1, const ObjDetectOutputBox& box2)
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

std::vector<ObjDetectOutputBox> NvVicResnet18TrafficCamNet::applyNMS(std::vector<ObjDetectOutputBox>& boxes,
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

void NvVicResnet18TrafficCamNet::parseDetectNetOutput(const float* coverageBuffer, const float* bboxBuffer,
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
    
    // 调试：查找最大置信度值
    float maxConfidence = -1.0f;
    int maxConfClass = -1;
    int maxConfIdx = -1;
    for (int classIdx = 0; classIdx < numClasses; classIdx++)
    {
        for (int i = 0; i < gridSize; i++)
        {
            float conf = coverageBuffer[classIdx * gridSize + i];
            if (conf > maxConfidence)
            {
                maxConfidence = conf;
                maxConfClass = classIdx;
                maxConfIdx = i;
            }
        }
    }
    std::cout << "[NvVicResnet18TrafficCamNet] Max confidence: " << maxConfidence 
              << " at class " << maxConfClass << ", grid index " << maxConfIdx 
              << " (threshold=" << params.conf_threshold << ")" << std::endl;

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

int NvVicResnet18TrafficCamNet::postProcess(const std::string& labelTextPath, const ObjDetectParams& params,
                                            std::vector<IDnnEngine::dnnOutput>& inputData, 
                                            std::vector<ObjDetectOutputBox>& outputData)
{
    // 验证输入
    if (inputData.size() != EXPECTED_OUTPUTS)
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] Error: Expected " << EXPECTED_OUTPUTS 
                  << " output tensors, got " << inputData.size() << std::endl;
        return -1;
    }

    // 加载标签
    if (!m_labelsLoaded && !labelTextPath.empty())
    {
        if (loadLabels(labelTextPath) != 0)
        {
            std::cerr << "[NvVicResnet18TrafficCamNet] Warning: Failed to load labels" << std::endl;
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
        std::cerr << "[NvVicResnet18TrafficCamNet] Error: Invalid output buffers" << std::endl;
        return -1;
    }

    // 推断网格尺寸和类别数
    // TrafficCamNet默认有4个类别
    const int numClasses = 4;  // Vehicle, TwoWheeler, Person, RoadSign

    // 计算网格尺寸
    int gridSize = coverageSize / numClasses;
    
    std::cout << "[NvVicResnet18TrafficCamNet] Coverage size: " << coverageSize 
              << " floats, BBox size: " << bboxSize << " floats" << std::endl;
    std::cout << "[NvVicResnet18TrafficCamNet] Grid cells: " << gridSize 
              << " (for " << numClasses << " classes)" << std::endl;
    
    // 根据模型输入尺寸和stride计算网格（ResNet18 TrafficCamNet使用stride=16）
    const int STRIDE = 16;
    int gridW = params.model_input_width / STRIDE;
    int gridH = params.model_input_height / STRIDE;
    
    std::cout << "[NvVicResnet18TrafficCamNet] Model input: " 
              << params.model_input_width << "x" << params.model_input_height << std::endl;
    std::cout << "[NvVicResnet18TrafficCamNet] Calculated grid (stride=" << STRIDE << "): " 
              << gridW << "x" << gridH << " = " << gridW * gridH << std::endl;

    // 验证尺寸
    if (gridW * gridH != gridSize)
    {
        std::cerr << "[NvVicResnet18TrafficCamNet] WARNING: Grid size mismatch! Expected: " 
                  << gridSize << ", Got: " << gridW * gridH << std::endl;
        
        // 尝试其他stride值
        for (int testStride = 8; testStride <= 32; testStride += 4)
        {
            int testW = params.model_input_width / testStride;
            int testH = params.model_input_height / testStride;
            if (testW * testH == gridSize)
            {
                std::cout << "[NvVicResnet18TrafficCamNet] Found matching stride: " << testStride 
                          << " -> " << testW << "x" << testH << std::endl;
                gridW = testW;
                gridH = testH;
                break;
            }
        }
    }
    
    // 解析DetectNet输出
    std::vector<ObjDetectOutputBox> detections;
    parseDetectNetOutput(coverageBuffer, bboxBuffer, gridW, gridH, numClasses, params, detections);
    
    std::cout << "[NvVicResnet18TrafficCamNet] Found " << detections.size() << " raw detections" << std::endl;
    
    // 应用NMS
    outputData = applyNMS(detections, params.nms_threshold);
    
    std::cout << "[NvVicResnet18TrafficCamNet] After NMS: " << outputData.size() << " detections" << std::endl;
    
    return 0;
}

} // namespace bsp_dnn

