#include "YoloPostProcess.hpp"
#include <iostream>
#include <fstream>
#include <numeric>
#include <algorithm>
#include <set>

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
    // Create a vector of pairs (value, index)
    std::vector<std::pair<float, int>> value_index_pairs;
    for (size_t i = 0; i < input.size(); ++i)
    {
        value_index_pairs.emplace_back(input[i], indices[i]);
    }

    // Sort the pairs based on the value in descending order
    std::sort(value_index_pairs.begin(), value_index_pairs.end(), [](const std::pair<float, int> &a, const std::pair<float, int> &b) {
        return a.first > b.first;
    });

    // Update the input and indices vectors based on the sorted pairs
    for (size_t i = 0; i < value_index_pairs.size(); ++i)
    {
        input[i] = value_index_pairs[i].first;
        indices[i] = value_index_pairs[i].second;
    }
}

template <typename T>
float YoloPostProcess::calculateOverlap(const bboxRect<T>& bbox1, const bboxRect<T>& bbox2)
{
    float w = fmax(0.f, fmin(bbox1.right, bbox2.right) - fmax(bbox1.left, bbox2.left) + 1.0);
    float h = fmax(0.f, fmin(bbox1.bottom, bbox2.bottom) - fmax(bbox1.top, bbox2.top) + 1.0);
    float i = w * h;
    float u = (bbox1.right - bbox1.left + 1.0) * (bbox1.bottom - bbox1.top + 1.0)
            + (bbox2.right - bbox2.left + 1.0) * (bbox2.bottom - bbox2.top + 1.0) - i;

    return u <= 0.f ? 0.f : (i / u);
}

int YoloPostProcess::nms(int validCount, std::vector<float> &filterBoxes, std::vector<int> classIds,
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
      float xmin0 = filterBoxes[n * 4 + 0];
      float ymin0 = filterBoxes[n * 4 + 1];
      float xmax0 = filterBoxes[n * 4 + 0] + filterBoxes[n * 4 + 2];
      float ymax0 = filterBoxes[n * 4 + 1] + filterBoxes[n * 4 + 3];

      float xmin1 = filterBoxes[m * 4 + 0];
      float ymin1 = filterBoxes[m * 4 + 1];
      float xmax1 = filterBoxes[m * 4 + 0] + filterBoxes[m * 4 + 2];
      float ymax1 = filterBoxes[m * 4 + 1] + filterBoxes[m * 4 + 3];

      float iou = calculateOverlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

      if (iou > threshold)
      {
        order[j] = -1;
      }
    }
  }
  return 0;

}


int YoloPostProcess::runPostProcess(const ObjDetectParams& params, std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData)
{
    std::vector<float> filterBoxes;
    std::vector<float> objScores;
    std::vector<int> classId;
    int validBoxNum = 0;

    for (int i = 0; i < inputData.size(); i++)
    {
        int stride = BASIC_STRIDE * (i + 1);
        validBoxNum += doProcess(params, stride, inputData[i], filterBoxes, objScores, classId);
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

}

int YoloPostProcess::doProcess(const ObjDetectParams& params, int stride, IDnnEngine::dnnOutput& inputData,
            std::vector<float>& bboxes, std::vector<float>& objScores, std::vector<int>& classId)
{

}


} // namespace bsp_dnn