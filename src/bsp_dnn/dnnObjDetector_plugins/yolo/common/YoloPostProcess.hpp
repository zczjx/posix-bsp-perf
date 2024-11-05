#ifndef __YOLO_POST_PROCESS_HPP__
#define __YOLO_POST_PROCESS_HPP__

#include <string>
#include <vector>
#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>

namespace bsp_dnn
{

class YoloPostProcess
{
public:
    static constexpr int BASIC_STRIDE = 8;
    static constexpr int MAX_OBJ_NUM = 64;
    YoloPostProcess() = default;
    ~YoloPostProcess() = default;

    YoloPostProcess(const YoloPostProcess&) = delete;
    YoloPostProcess& operator=(const YoloPostProcess&) = delete;
    YoloPostProcess(YoloPostProcess&&) = delete;
    YoloPostProcess& operator=(YoloPostProcess&&) = delete;

    int initLabelMap(const std::string& labelMapPath);
    int runPostProcess(const ObjDetectParams& params, std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData);

private:
    int doProcess(const ObjDetectParams& params, int stride, IDnnEngine::dnnOutput& inputData,
            std::vector<float>& bboxes, std::vector<float>& objScores, std::vector<int>& classId);

    void inverseSortWithIndices(std::vector<float> &input, std::vector<int> &indices);

    int nms(int validCount, std::vector<float> &filterBoxes, std::vector<int> classIds,
            std::vector<int> &order, int filterId, float threshold);

    inline int clamp(float val, int min, int max)
    {
        return val > min ? (val < max ? val : max) : min;
    }

    template <typename T>
    float calculateOverlap(const bboxRect<T>& bbox1, const bboxRect<T>& bbox2);


private:
    std::vector<std::string> m_labelMap;
    bool m_labelMapInited{false};
};



} // namespace bsp_dnn

#endif // __YOLO_POST_PROCESS_HPP__