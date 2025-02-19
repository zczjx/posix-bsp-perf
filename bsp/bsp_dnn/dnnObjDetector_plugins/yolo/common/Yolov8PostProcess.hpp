#ifndef __YOLOV8_POST_PROCESS_HPP__
#define __YOLOV8_POST_PROCESS_HPP__

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <cmath>
#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>

namespace bsp_dnn
{

class Yolov8PostProcess
{
public:
    static constexpr int BASIC_STRIDE = 8;
    static constexpr int MAX_OBJ_NUM = 64;
    static constexpr int OBJ_CLASS_NUM = 80;
    static constexpr int PROP_BOX_SIZE = 5 + OBJ_CLASS_NUM;

    Yolov8PostProcess() = default;
    ~Yolov8PostProcess() = default;
    Yolov8PostProcess(const Yolov8PostProcess&) = delete;
    Yolov8PostProcess& operator=(const Yolov8PostProcess&) = delete;
    Yolov8PostProcess(Yolov8PostProcess&&) = delete;
    Yolov8PostProcess& operator=(Yolov8PostProcess&&) = delete;

    int initLabelMap(const std::string& labelMapPath);
    int runPostProcess(const ObjDetectParams& params, std::vector<IDnnEngine::dnnOutput>& inputData, std::vector<ObjDetectOutputBox>& outputData);

private:
    int doProcess(const int idx, const ObjDetectParams& params, int stride, IDnnEngine::dnnOutput& inputData,
            std::vector<float>& bboxes, std::vector<float>& objScores, std::vector<int>& classId);

    int process_i8(int8_t *box_tensor, int32_t box_zp, float box_scale,
            int8_t *score_tensor, int32_t score_zp, float score_scale,
            int8_t *score_sum_tensor, int32_t score_sum_zp, float score_sum_scale,
            int grid_h, int grid_w, int stride, int dfl_len,
            std::vector<float> &boxes, 
            std::vector<float> &objProbs, 
            std::vector<int> &classId, 
            float threshold)
    
    
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


    int8_t qauntFP32ToAffine(float fp32, int32_t zp, float scale);

    float deqauntAffineToFP32(int8_t qnt, int32_t zp, float scale) { return ((float)qnt - (float)zp) * scale; }

    inline int32_t clip(float val, int32_t min, int32_t max)
    {
        float f = val <= min ? min : (val >= max ? max : val);
        return f;
    }

    static void compute_dfl(float* tensor, int dfl_len, float* box){
        for (int b=0; b<4; b++){
            float exp_t[dfl_len];
            float exp_sum=0;
            float acc_sum=0;
            for (int i=0; i< dfl_len; i++){
                exp_t[i] = exp(tensor[i+b*dfl_len]);
                exp_sum += exp_t[i];
            }
            
            for (int i=0; i< dfl_len; i++){
                acc_sum += exp_t[i]/exp_sum *i;
            }
            box[b] = acc_sum;
        }
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