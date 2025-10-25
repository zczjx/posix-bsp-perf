# ResNet18 TrafficCamNet Plugin

## 概述

这是一个基于TensorRT的ResNet18 TrafficCamNet目标检测插件，用于检测交通场景中的物体。

## 支持的类别

该模型可以检测以下4个类别：
1. **Vehicle** (车辆) - Class 0
2. **TwoWheeler** (两轮车) - Class 1  
3. **Person** (行人) - Class 2
4. **RoadSign** (路标) - Class 3

## 模型信息

- **模型架构**: ResNet18
- **模型名称**: TrafficCamNet (pruned version)
- **输入格式**: RGB, 浮点型, CHW planar (NCHW)
- **输入尺寸**: 通常为 [1, 3, 544, 960] 或 [1, 3, 368, 640]
- **输出格式**: DetectNet 架构
  - Coverage层: [num_classes, grid_h, grid_w] - 置信度分数
  - BBox层: [num_classes*4, grid_h, grid_w] - 边界框坐标 (x1, y1, x2, y2)
- **归一化**: scale_factor = 1/255 = 0.00392156862745098

## 预处理步骤

1. **BGR到RGB转换**: OpenCV默认使用BGR格式，需要转换为RGB
2. **Letterbox Resize**: 保持宽高比的缩放，使用灰色填充 (128, 128, 128)
3. **归一化**: 像素值除以255
4. **格式转换**: 从HWC格式转换为CHW planar格式

## 后处理步骤

1. **解析DetectNet输出**: 
   - 解码coverage层获取置信度
   - 解码bbox层获取边界框坐标
   - 应用归一化因子 (35.0, 35.0)
2. **置信度阈值过滤**: 过滤低置信度检测
3. **坐标转换**: 从模型空间转换回原始图像空间
4. **NMS (非极大值抑制)**: 去除重复检测

## 编译

```bash
cd /build/clarencez/posix-bsp-perf/bsp/bsp_dnn/dnnObjDetector_plugins/resnet18
mkdir build && cd build
cmake ..
make
```

编译后会生成共享库: `libresnet18_trafficcamnet.so`

## 使用方法

### C++ 示例

```cpp
#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>
#include <bsp_dnn/IDnnEngine.hpp>
#include <opencv2/opencv.hpp>
#include <memory>
#include <dlfcn.h>

int main()
{
    // 1. 加载插件
    void* handle = dlopen("./libresnet18_trafficcamnet.so", RTLD_LAZY);
    if (!handle) {
        std::cerr << "Cannot load plugin: " << dlerror() << std::endl;
        return -1;
    }
    
    auto create_plugin = (bsp_dnn::IDnnObjDetectorPlugin* (*)())dlsym(handle, "create");
    auto destroy_plugin = (void (*)(bsp_dnn::IDnnObjDetectorPlugin*))dlsym(handle, "destroy");
    
    auto plugin = create_plugin();
    
    // 2. 创建TensorRT推理引擎
    auto engine = bsp_dnn::IDnnEngine::create("trt");
    engine->loadModel("resnet18_trafficcamnet_pruned.onnx");
    
    // 3. 获取输入形状
    bsp_dnn::IDnnEngine::dnnInputShape shape;
    engine->getInputShape(shape);
    
    // 4. 设置检测参数
    bsp_dnn::ObjDetectParams params;
    params.model_input_width = shape.width;
    params.model_input_height = shape.height;
    params.model_input_channel = shape.channel;
    params.conf_threshold = 0.2f;      // 置信度阈值
    params.nms_threshold = 0.5f;       // NMS IoU阈值
    
    // 5. 加载输入图像
    cv::Mat image = cv::imread("test_image.jpg");
    auto imagePtr = std::make_shared<cv::Mat>(image);
    
    // 6. 预处理
    bsp_dnn::ObjDetectInput input;
    input.handleType = "opencv4";
    input.imageHandle = imagePtr;
    
    bsp_dnn::IDnnEngine::dnnInput engineInput;
    plugin->preProcess(params, input, engineInput);
    
    // 7. 推理
    engine->pushInputData(engineInput);
    engine->runInference();
    
    std::vector<bsp_dnn::IDnnEngine::dnnOutput> outputs;
    engine->popOutputData(outputs);
    
    // 8. 后处理
    std::vector<bsp_dnn::ObjDetectOutputBox> detections;
    plugin->postProcess("labels.txt", params, outputs, detections);
    
    // 9. 显示结果
    for (const auto& det : detections) {
        cv::rectangle(image, 
                     cv::Point(det.bbox.left, det.bbox.top),
                     cv::Point(det.bbox.right, det.bbox.bottom),
                     cv::Scalar(0, 255, 0), 2);
        
        std::string text = det.label + ": " + std::to_string(det.score);
        cv::putText(image, text, 
                   cv::Point(det.bbox.left, det.bbox.top - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
        
        std::cout << "Detected: " << det.label 
                  << " (confidence: " << det.score << ")" << std::endl;
    }
    
    cv::imwrite("output.jpg", image);
    
    // 10. 清理
    engine->releaseOutputData(outputs);
    destroy_plugin(plugin);
    dlclose(handle);
    
    return 0;
}
```

### 标签文件格式 (labels.txt)

```
Vehicle
TwoWheeler
Person
RoadSign
```

## 配置参数

### ObjDetectParams 主要参数：

- `model_input_width`: 模型输入宽度 (例如: 960)
- `model_input_height`: 模型输入高度 (例如: 544)
- `model_input_channel`: 输入通道数 (固定为 3)
- `conf_threshold`: 置信度阈值，推荐值: 0.2
- `nms_threshold`: NMS IoU阈值，推荐值: 0.5
- `scale_width`: 宽度缩放因子 (由预处理自动计算)
- `scale_height`: 高度缩放因子 (由预处理自动计算)
- `pads`: Letterbox填充尺寸 (由预处理自动计算)

## 性能优化建议

1. **批处理**: TensorRT引擎支持批处理以提高吞吐量
2. **INT8量化**: 使用INT8精度可以显著提高推理速度
3. **固定输入尺寸**: 使用固定输入尺寸可以避免动态形状开销
4. **预分配内存**: 重用输入/输出缓冲区以减少内存分配

## 与DeepStream配置对应关系

从DeepStream配置 `dstest1_pgie_config.txt` 到插件参数的映射：

| DeepStream参数 | 插件参数 | 说明 |
|---------------|---------|------|
| `net-scale-factor` | 内置于预处理 | 固定为 1/255 |
| `onnx-file` | `engine->loadModel()` | ONNX模型路径 |
| `labelfile-path` | `postProcess()` | 标签文件路径 |
| `num-detected-classes` | 固定为 4 | 检测类别数 |
| `pre-cluster-threshold` | `conf_threshold` | 置信度阈值 |
| `nms-iou-threshold` | `nms_threshold` | NMS IoU阈值 |
| `network-mode=1` | TensorRT配置 | INT8模式 |

## 故障排除

### 常见问题

1. **检测结果为空**
   - 检查置信度阈值是否设置过高
   - 验证输入图像是否正确加载
   - 确认模型文件路径正确

2. **坐标不准确**
   - 确认模型输入尺寸与实际输入一致
   - 检查letterbox padding计算是否正确
   - 验证scale参数是否正确保存

3. **编译错误**
   - 确保OpenCV已正确安装
   - 检查CMake配置中的路径设置
   - 验证IDnnObjDetectorPlugin.hpp可访问

## 参考

- DeepStream示例: `/build/clarencez/posix-bsp-perf/deepstream/sources/apps/sample_apps/deepstream-test1`
- NVIDIA DeepStream文档: https://docs.nvidia.com/metropolis/deepstream/
- ResNet18 TrafficCamNet模型: NVIDIA NGC模型库

