#ifndef __YOLO_POST_PROCESS_HPP__
#define __YOLO_POST_PROCESS_HPP__

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <cmath>
#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>

namespace bsp_dnn
{

class YoloPostProcess
{
public:
    static constexpr int BASIC_STRIDE = 8;
    static constexpr int MAX_OBJ_NUM = 64;
    static constexpr int OBJ_CLASS_NUM = 80;
    static constexpr int PROP_BOX_SIZE = 5 + OBJ_CLASS_NUM;

    YoloPostProcess() = default;
    ~YoloPostProcess() = default;
    YoloPostProcess(const YoloPostProcess&) = delete;
    YoloPostProcess& operator=(const YoloPostProcess&) = delete;
    YoloPostProcess(YoloPostProcess&&) = delete;
    YoloPostProcess& operator=(YoloPostProcess&&) = delete;

    int initLabelMap(const std::string& labelMapPath);
    int runPostProcess(const ObjDetectParams& params, std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData);

private:
    int doProcess(const int idx, const ObjDetectParams& params, int stride, IDnnEngine::dnnOutput& inputData,
            std::vector<float>& bboxes, std::vector<float>& objScores, std::vector<int>& classId);

    void inverseSortWithIndices(std::vector<float> &input, std::vector<int> &indices);

    int nms(int validCount, std::vector<float> &filterBoxes, std::vector<int> classIds,
            std::vector<int> &order, int filterId, float threshold);

    inline int clamp(float val, int min, int max)
    {
        return val > min ? (val < max ? val : max) : min;
    }

    template <typename T>
    float calculateOverlap(const bboxRect<T>& bbox1, const bboxRect<T>& bbox2)
    {
        float w = fmax(0.f, fmin(bbox1.right, bbox2.right) - fmax(bbox1.left, bbox2.left) + 1.0);
        float h = fmax(0.f, fmin(bbox1.bottom, bbox2.bottom) - fmax(bbox1.top, bbox2.top) + 1.0);
        float i = w * h;
        float u = (bbox1.right - bbox1.left + 1.0) * (bbox1.bottom - bbox1.top + 1.0)
                + (bbox2.right - bbox2.left + 1.0) * (bbox2.bottom - bbox2.top + 1.0) - i;

        return u <= 0.f ? 0.f : (i / u);
    }


    int8_t qauntFP32ToAffine(float fp32, int8_t zp, float scale);

    float deqauntAffineToFP32(int8_t qnt, int32_t zp, float scale) { return ((float)qnt - (float)zp) * scale; }

    inline int32_t clip(int32_t val, int32_t min, int32_t max)
    {
        float f = val <= min ? min : (val >= max ? max : val);
        return f;
    }


private:
    std::vector<std::string> m_labelMap;
    bool m_labelMapInited{false};
    std::vector<std::array<const int, 6>> m_anchorVec = {
        {10, 13, 16, 30, 33, 23},
        {30, 61, 62, 45, 59, 119},
        {116, 90, 156, 198, 373, 326}
    };
};



} // namespace bsp_dnn

#endif // __YOLO_POST_PROCESS_HPP__