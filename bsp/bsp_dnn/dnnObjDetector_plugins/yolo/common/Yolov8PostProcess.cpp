#include "Yolov8PostProcess.hpp"
#include <iostream>
#include <numeric>
#include <algorithm>
#include <set>
#include <fstream>

namespace bsp_dnn
{

int Yolov8PostProcess::initLabelMap(const std::string& labelMapPath)
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

void Yolov8PostProcess::inverseSortWithIndices(std::vector<float> &input, std::vector<int> &indices)
{
    // Create a vector of pairs (value, index)
    std::vector<std::pair<float, int>> value_index_pairs;
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
}

int Yolov8PostProcess::nms(int validCount, std::vector<float> &filterBoxes, std::vector<int> classIds,
        std::vector<int> &order, int filterId, float threshold)
{

    for (int i = 0; i < validCount; ++i)
    {
        int n = order[i];
        if (n == -1 || classIds[n] != filterId)
        {
            continue;
        }

        for (int j = i + 1; j < validCount; ++j)
        {
            int m = order[j];
            if (m == -1 || classIds[m] != filterId)
            {
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

}

int Yolov8PostProcess::runPostProcess(const ObjDetectParams& params, std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData)
{
    std::vector<float> filterBoxes;
    std::vector<float> objScores;
    std::vector<int> classId;
    int validBoxNum = 0;
    int grid_h = 0;
    int grid_w = 0;
    int stride = 0;
    int model_in_w = params.model_input_width;
    int model_in_h = params.model_input_height;
    int dfl_len = params.output_attrs[0].dims[1]/4;
    int output_per_branch = params.io_num.n_output / 3;
    int conf_threshold = params.conf_threshold;

    for (int i = 0; i < 3; i++)
    {
        void *score_sum = nullptr;
        int32_t score_sum_zp = 0;
        float score_sum_scale = 1.0;
        if (output_per_branch == 3)
        {
            score_sum = inputData[i*output_per_branch + 2].buf;
            score_sum_zp = params.output_attrs[i*output_per_branch + 2].zp;
            score_sum_scale = params.output_attrs[i*output_per_branch + 2].scale;
        }
        int box_idx = i * output_per_branch;
        int score_idx = i * output_per_branch +1;
        
        grid_h = params.output_attrs[box_idx].dims[2];
        grid_w = params.output_attrs[box_idx].dims[3];
    
        stride = model_in_h / grid_h;

        validBoxNum += process_i8((int8_t *)inputData[box_idx].buf, params.output_attrs[box_idx].zp, params.output_attrs[box_idx].scale,
                                (int8_t *)inputData[score_idx].buf, params.output_attrs[score_idx].zp, params.output_attrs[score_idx].scale,
                                (int8_t *)score_sum, score_sum_zp, score_sum_scale,
                                grid_h, grid_w, stride, dfl_len, 
                                filterBoxes, objScores, classId, conf_threshold);
    }

    if (validBoxNum <= 0)
    {
        return 0;
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

        float x1 = filterBoxes[n * 4 + 0] - params.pads.left;
        float y1 = filterBoxes[n * 4 + 1] - params.pads.top;
        float x2 = x1 + filterBoxes[n * 4 + 2];
        float y2 = y1 + filterBoxes[n * 4 + 3];
        outputBox.bbox.left = (int)(clamp(x1, 0, params.model_input_width) / params.scale_width);
        outputBox.bbox.top = (int)(clamp(y1, 0, params.model_input_height) / params.scale_height);
        outputBox.bbox.right = (int)(clamp(x2, 0, params.model_input_width) / params.scale_width);
        outputBox.bbox.bottom = (int)(clamp(y2, 0, params.model_input_height) / params.scale_height);
        outputBox.score = objScores[i];
        int id = classId[n];
        outputBox.label = m_labelMap[id];
        outputData.push_back(outputBox);

        detectObjCount++;
    }

    return 0;
}

int8_t Yolov8PostProcess::qauntFP32ToAffine(float fp32, int32_t zp, float scale)
{
    float dst_val = (fp32 / scale) + zp;
    int8_t res = (int8_t)clip(dst_val, -128, 127);
    return res;
}

int Yolov8PostProcess::process_i8(int8_t *box_tensor, int32_t box_zp, float box_scale,
    int8_t *score_tensor, int32_t score_zp, float score_scale,
    int8_t *score_sum_tensor, int32_t score_sum_zp, float score_sum_scale,
    int grid_h, int grid_w, int stride, int dfl_len,
    std::vector<float> &boxes, 
    std::vector<float> &objProbs, 
    std::vector<int> &classId, 
    float threshold)
{
    int validCount = 0;
    int grid_len = grid_h * grid_w;
    int8_t score_thres_i8 = qauntFP32ToAffine(threshold, score_zp, score_scale);
    int8_t score_sum_thres_i8 = qauntFP32ToAffine(threshold, score_sum_zp, score_sum_scale);

    for (int i = 0; i < grid_h; i++)
    {
        for (int j = 0; j < grid_w; j++)
        {
            int offset = i* grid_w + j;
            int max_class_id = -1;

            // 通过 score sum 起到快速过滤的作用
            if (score_sum_tensor != nullptr){
                if (score_sum_tensor[offset] < score_sum_thres_i8){
                    continue;
                }
            }

            int8_t max_score = -score_zp;
            for (int c= 0; c< OBJ_CLASS_NUM; c++){
                if ((score_tensor[offset] > score_thres_i8) && (score_tensor[offset] > max_score))
                {
                    max_score = score_tensor[offset];
                    max_class_id = c;
                }
                offset += grid_len;
            }

            // compute box
            if (max_score> score_thres_i8){
                offset = i* grid_w + j;
                float box[4];
                float before_dfl[dfl_len*4];
                for (int k=0; k< dfl_len*4; k++){
                    before_dfl[k] = deqauntAffineToFP32(box_tensor[offset], box_zp, box_scale);
                    offset += grid_len;
                }
                compute_dfl(before_dfl, dfl_len, box);

                float x1,y1,x2,y2,w,h;
                x1 = (-box[0] + j + 0.5)*stride;
                y1 = (-box[1] + i + 0.5)*stride;
                x2 = (box[2] + j + 0.5)*stride;
                y2 = (box[3] + i + 0.5)*stride;
                w = x2 - x1;
                h = y2 - y1;
                boxes.push_back(x1);
                boxes.push_back(y1);
                boxes.push_back(w);
                boxes.push_back(h);

                objProbs.push_back(deqauntAffineToFP32(max_score, score_zp, score_scale));
                classId.push_back(max_class_id);
                validCount ++;
            }
        }
    }
    return validCount;
}

int Yolov8PostProcess::doProcess(const int idx, const ObjDetectParams& params, int stride, IDnnEngine::dnnOutput& inputData,
            std::vector<float>& bboxes, std::vector<float>& objScores, std::vector<int>& classId)
{
    int validCount = 0;
    int grid_h = params.model_input_height / stride;
    int grid_w = params.model_input_width / stride;
    int grid_len = grid_h * grid_w;

    int8_t thres_i8 = qauntFP32ToAffine(params.conf_threshold, params.quantize_zero_points[idx],
                        params.quantize_scales[idx]);

    int8_t *input_buf = (int8_t *)inputData.buf;

    for (int a = 0; a < 3; a++)
    {
        for (int i = 0; i < grid_h; i++)
        {
            for (int j = 0; j < grid_w; j++)
            {
                int8_t box_confidence = input_buf[(PROP_BOX_SIZE * a + 4) * grid_len + i * grid_w + j];
                if (box_confidence >= thres_i8)
                {
                    int offset = (PROP_BOX_SIZE * a) * grid_len + i * grid_w + j;
                    int8_t *in_ptr = input_buf + offset;
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
                    for (int k = 1; k < OBJ_CLASS_NUM; ++k)
                    {
                        int8_t prob = in_ptr[(5 + k) * grid_len];
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