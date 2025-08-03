#include "YoloPostProcess.hpp"
#include <iostream>
#include <numeric>
#include <algorithm>
#include <set>
#include <fstream>

namespace bsp_dnn
{

int YoloPostProcess::initLabelMap(const std::string& labelMapPath)
{
    if (m_labelMapInited)
    {
        return 0;
    }

    if (labelMapPath.empty())
    {
        return -1;
    }

    std::cout << "loading label path: " << labelMapPath << std::endl;

    std::ifstream labelFile(labelMapPath);
    if (!labelFile.is_open())
    {
        std::cerr << "Failed to open label map file: " << labelMapPath << std::endl;
        return -1;
    }

    std::string line;
    while (std::getline(labelFile, line))
    {
        m_labelMap.push_back(line);
    }

    labelFile.close();
    m_labelMapInited = true;
    return 0;
}

void YoloPostProcess::inverseSortWithIndices(std::vector<float> &input, std::vector<int> &indices)
{
    // 验证输入参数
    if (input.empty() || indices.empty()) {
        std::cerr << "YoloPostProcess::inverseSortWithIndices() empty input vectors" << std::endl;
        return;
    }

    if (input.size() != indices.size()) {
        std::cerr << "YoloPostProcess::inverseSortWithIndices() size mismatch: input="
                  << input.size() << ", indices=" << indices.size() << std::endl;
        return;
    }

    try {
        // Create a vector of pairs (value, index)
        std::vector<std::pair<float, int>> value_index_pairs;
        value_index_pairs.reserve(input.size()); // 预分配内存以避免重新分配

        for (size_t i = 0; i < input.size(); ++i)
        {
            value_index_pairs.emplace_back(input[i], indices[i]);
        }

        // Sort the pairs based on the value in descending order
        std::sort(value_index_pairs.begin(), value_index_pairs.end(), [](const std::pair<float, int> &a, const std::pair<float, int> &b) {
            return a.first >= b.first;
        });

        // Update the input and indices vectors based on the sorted pairs
        for (size_t i = 0; i < value_index_pairs.size(); ++i)
        {
            input[i] = value_index_pairs[i].first;
            indices[i] = value_index_pairs[i].second;
        }
    } catch (const std::exception& e) {
        std::cerr << "YoloPostProcess::inverseSortWithIndices() exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "YoloPostProcess::inverseSortWithIndices() unknown exception" << std::endl;
    }
}

int YoloPostProcess::nms(int validCount, std::vector<float> &filterBoxes, std::vector<int> classIds,
        std::vector<int> &order, int filterId, float threshold)
{
    // 验证输入参数
    if (validCount <= 0)
    {
        std::cerr << "YoloPostProcess::nms() invalid validCount: " << validCount << std::endl;
        return -1;
    }

    if (filterBoxes.size() < validCount * 4)
    {
        std::cerr << "YoloPostProcess::nms() filterBoxes too small: size="
                  << filterBoxes.size() << ", required=" << validCount * 4 << std::endl;
        return -1;
    }

    if (classIds.size() < validCount)
    {
        std::cerr << "YoloPostProcess::nms() classIds too small: size="
                  << classIds.size() << ", required=" << validCount << std::endl;
        return -1;
    }

    if (order.size() < validCount)
    {
        std::cerr << "YoloPostProcess::nms() order too small: size="
                  << order.size() << ", required=" << validCount << std::endl;
        return -1;
    }

    try {
        for (int i = 0; i < validCount; ++i)
        {
            int n = order[i];
            if (n == -1 || n >= validCount || classIds[n] != filterId)
            {
                continue;
            }

            for (int j = i + 1; j < validCount; ++j)
            {
                int m = order[j];
                if (m == -1 || m >= validCount || classIds[m] != filterId)
                {
                    continue;
                }

                // 验证边界框索引的有效性
                if (n * 4 + 3 >= filterBoxes.size() || m * 4 + 3 >= filterBoxes.size()) {
                    std::cerr << "YoloPostProcess::nms() bbox index out of bounds" << std::endl;
                    continue;
                }

                bboxRect<float> bbox1;
                bbox1.left = filterBoxes[n * 4 + 0];
                bbox1.top = filterBoxes[n * 4 + 1];
                bbox1.right = filterBoxes[n * 4 + 0] + filterBoxes[n * 4 + 2];
                bbox1.bottom = filterBoxes[n * 4 + 1] + filterBoxes[n * 4 + 3];
                bboxRect<float> bbox2;
                bbox2.left = filterBoxes[m * 4 + 0];
                bbox2.top = filterBoxes[m * 4 + 1];
                bbox2.right = filterBoxes[m * 4 + 0] + filterBoxes[m * 4 + 2];
                bbox2.bottom = filterBoxes[m * 4 + 1] + filterBoxes[m * 4 + 3];
                float iou = calculateOverlap(bbox1, bbox2);

                if (iou > threshold)
                {
                    order[j] = -1;
                }
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "YoloPostProcess::nms() exception: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "YoloPostProcess::nms() unknown exception" << std::endl;
        return -1;
    }
}

int YoloPostProcess::runPostProcess(const ObjDetectParams& params, std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData)
{
    // 验证输入参数
    if (inputData.empty()) {
        std::cerr << "YoloPostProcess::runPostProcess() empty input data" << std::endl;
        return 0;
    }

    try {
        std::vector<float> filterBoxes;
        std::vector<float> objScores;
        std::vector<int> classId;
        int validBoxNum = 0;

        for (int i = 0; i < inputData.size(); i++)
        {
            int stride = BASIC_STRIDE * (1 << i);
            int result = doProcess(i, params, stride, inputData[i], filterBoxes, objScores, classId);
            if (result < 0) {
                std::cerr << "YoloPostProcess::runPostProcess() doProcess failed for index " << i << std::endl;
                return -1;
            }
            validBoxNum += result;
        }

        if (validBoxNum <= 0)
        {
            return 0;
        }

        // 验证处理结果的一致性
        if (objScores.size() != validBoxNum || classId.size() != validBoxNum || filterBoxes.size() != validBoxNum * 4) {
            std::cerr << "YoloPostProcess::runPostProcess() data inconsistency: validBoxNum=" << validBoxNum 
                      << ", objScores.size=" << objScores.size() 
                      << ", classId.size=" << classId.size() 
                      << ", filterBoxes.size=" << filterBoxes.size() << std::endl;
            return -1;
        }

        std::vector<int> indexArray(validBoxNum);
        std::iota(indexArray.begin(), indexArray.end(), 0);
        inverseSortWithIndices(objScores, indexArray);
        std::set<int> class_set(std::begin(classId), std::end(classId));

        for(auto filterId : class_set)
        {
            nms(validBoxNum, filterBoxes, classId, indexArray, filterId, params.nms_threshold);
        }

        int detectObjCount = 0;
        for (int i = 0; i < validBoxNum; i++)
        {
            ObjDetectOutputBox outputBox;
            if (indexArray[i] == -1 || detectObjCount >= MAX_OBJ_NUM)
            {
                continue;
            }

            int n = indexArray[i];

            // 验证索引的有效性
            if (n < 0 || n >= validBoxNum || n * 4 + 3 >= filterBoxes.size()) {
                std::cerr << "YoloPostProcess::runPostProcess() invalid index: " << n << std::endl;
                continue;
            }

            float x1 = filterBoxes[n * 4 + 0] - params.pads.left;
            float y1 = filterBoxes[n * 4 + 1] - params.pads.top;
            float x2 = x1 + filterBoxes[n * 4 + 2];
            float y2 = y1 + filterBoxes[n * 4 + 3];
            outputBox.bbox.left = (int)(clamp(x1, 0, params.model_input_width) / params.scale_width);
            outputBox.bbox.top = (int)(clamp(y1, 0, params.model_input_height) / params.scale_height);
            outputBox.bbox.right = (int)(clamp(x2, 0, params.model_input_width) / params.scale_width);
            outputBox.bbox.bottom = (int)(clamp(y2, 0, params.model_input_height) / params.scale_height);
            outputBox.score = objScores[i];

            // 验证类别ID的有效性
            if (n < classId.size()) {
                int id = classId[n];
                if (id >= 0 && id < m_labelMap.size()) {
                    outputBox.label = m_labelMap[id];
                } else {
                    std::cerr << "YoloPostProcess::runPostProcess() invalid class ID: " << id << std::endl;
                    outputBox.label = "unknown";
                }
            } else {
                std::cerr << "YoloPostProcess::runPostProcess() class ID index out of bounds" << std::endl;
                outputBox.label = "unknown";
            }
            outputData.push_back(outputBox);
            detectObjCount++;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "YoloPostProcess::runPostProcess() exception: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "YoloPostProcess::runPostProcess() unknown exception" << std::endl;
        return -1;
    }
}

int8_t YoloPostProcess::qauntFP32ToAffine(float fp32, int8_t zp, float scale)
{
    float dst_val = (fp32 / scale) + zp;
    int8_t res = (int8_t)clip(dst_val, -128, 127);
    return res;
}


int YoloPostProcess::doProcess(const int idx, const ObjDetectParams& params, int stride, IDnnEngine::dnnOutput& inputData,
            std::vector<float>& bboxes, std::vector<float>& objScores, std::vector<int>& classId)
{
    int validCount = 0;
    int grid_h = params.model_input_height / stride;
    int grid_w = params.model_input_width / stride;
    int grid_len = grid_h * grid_w;

    // 验证输入参数的有效性
    if (grid_h <= 0 || grid_w <= 0 || grid_len <= 0) {
        std::cerr << "YoloPostProcess::doProcess() invalid grid dimensions: "
                  << grid_h << "x" << grid_w << ", grid_len=" << grid_len << std::endl;
        return 0;
    }

    // 验证输入缓冲区
    if (!inputData.buf || inputData.size <= 0) {
        std::cerr << "YoloPostProcess::doProcess() invalid input buffer: buf="
                  << inputData.buf << ", size=" << inputData.size << std::endl;
        return 0;
    }

    // 计算所需的最小缓冲区大小
    size_t required_size = PROP_BOX_SIZE * 3 * grid_len;
    if (inputData.size < required_size) {
        std::cerr << "YoloPostProcess::doProcess() buffer too small: required="
                  << required_size << ", actual=" << inputData.size << std::endl;
        return 0;
    }

    int8_t thres_i8 = qauntFP32ToAffine(params.conf_threshold, params.quantize_zero_points[idx],
                        params.quantize_scales[idx]);

    int8_t *input_buf = (int8_t *)inputData.buf;

    for (int a = 0; a < 3; a++)
    {
        for (int i = 0; i < grid_h; i++)
        {
            for (int j = 0; j < grid_w; j++)
            {
                // 计算索引并验证边界
                size_t confidence_idx = (PROP_BOX_SIZE * a + 4) * grid_len + i * grid_w + j;
                if (confidence_idx >= inputData.size) {
                    std::cerr << "YoloPostProcess::doProcess() confidence index out of bounds: "
                              << confidence_idx << " >= " << inputData.size << std::endl;
                    continue;
                }

                int8_t box_confidence = input_buf[confidence_idx];
                if (box_confidence >= thres_i8)
                {
                    int offset = (PROP_BOX_SIZE * a) * grid_len + i * grid_w + j;
                    // 验证offset的有效性
                    if (offset < 0 || offset >= inputData.size) {
                        std::cerr << "YoloPostProcess::doProcess() invalid offset: " << offset << std::endl;
                        continue;
                    }

                    int8_t *in_ptr = input_buf + offset;

                    // 验证所有访问点的边界
                    if (offset + 5 * grid_len >= inputData.size) {
                        std::cerr << "YoloPostProcess::doProcess() buffer access out of bounds" << std::endl;
                        continue;
                    }

                    float box_x = (deqauntAffineToFP32(*in_ptr, params.quantize_zero_points[idx], params.quantize_scales[idx])) * 2.0 - 0.5;
                    float box_y = (deqauntAffineToFP32(in_ptr[grid_len], params.quantize_zero_points[idx], params.quantize_scales[idx])) * 2.0 - 0.5;
                    float box_w = (deqauntAffineToFP32(in_ptr[2 * grid_len], params.quantize_zero_points[idx], params.quantize_scales[idx])) * 2.0;
                    float box_h = (deqauntAffineToFP32(in_ptr[3 * grid_len], params.quantize_zero_points[idx], params.quantize_scales[idx])) * 2.0;
                    box_x = (box_x + j) * (float) stride;
                    box_y = (box_y + i) * (float) stride;
                    box_w = box_w * box_w * (float) m_anchorVec[idx][a * 2];
                    box_h = box_h * box_h * (float) m_anchorVec[idx][a * 2 + 1];
                    box_x -= (box_w / 2.0);
                    box_y -= (box_h / 2.0);

                    int8_t maxClassProbs = in_ptr[5 * grid_len];
                    int maxClassId = 0;
                    // 验证类别数量访问的边界
                    for (int k = 1; k < OBJ_CLASS_NUM; ++k)
                    {
                        size_t prob_idx = (5 + k) * grid_len;
                        if (offset + prob_idx >= inputData.size) {
                            std::cerr << "YoloPostProcess::doProcess() class prob index out of bounds" << std::endl;
                            break;
                        }
                        int8_t prob = in_ptr[prob_idx];
                        if (prob > maxClassProbs)
                        {
                            maxClassId = k;
                            maxClassProbs = prob;
                        }
                    }

                    if (maxClassProbs > thres_i8)
                    {
                        objScores.push_back((deqauntAffineToFP32(maxClassProbs, params.quantize_zero_points[idx], params.quantize_scales[idx]))
                                    * (deqauntAffineToFP32(box_confidence, params.quantize_zero_points[idx], params.quantize_scales[idx])));
                        classId.push_back(maxClassId);
                        validCount++;
                        bboxes.push_back(box_x);
                        bboxes.push_back(box_y);
                        bboxes.push_back(box_w);
                        bboxes.push_back(box_h);
                    }
                }
            }
        }
    }
    return validCount;
}


} // namespace bsp_dnn