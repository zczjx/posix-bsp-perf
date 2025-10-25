/**
 * @file example_usage.cpp
 * @brief 使用ResNet18 TrafficCamNet插件的完整示例
 * 
 * 编译命令:
 * g++ -std=c++17 example_usage.cpp -o example_usage \
 *     -I../../../.. \
 *     -L./build -lresnet18_trafficcamnet \
 *     -L../../../../build/lib -lbsp_dnn \
 *     `pkg-config --cflags --libs opencv4` \
 *     -ldl -pthread
 */

#include <bsp_dnn/IDnnObjDetectorPlugin.hpp>
#include <bsp_dnn/IDnnEngine.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <memory>
#include <dlfcn.h>

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <image_path> [label_path]" << std::endl;
        std::cerr << "Example: " << argv[0] 
                  << " resnet18_trafficcamnet.onnx test.jpg labels.txt" << std::endl;
        return -1;
    }

    std::string modelPath = argv[1];
    std::string imagePath = argv[2];
    std::string labelPath = (argc > 3) ? argv[3] : "labels.txt";

    std::cout << "=== ResNet18 TrafficCamNet Detection Demo ===" << std::endl;
    std::cout << "Model: " << modelPath << std::endl;
    std::cout << "Image: " << imagePath << std::endl;
    std::cout << "Labels: " << labelPath << std::endl;
    std::cout << std::endl;

    // 步骤1: 加载插件
    std::cout << "[1/9] Loading plugin..." << std::endl;
    void* pluginHandle = dlopen("./libresnet18_trafficcamnet.so", RTLD_LAZY);
    if (!pluginHandle) {
        std::cerr << "Error: Cannot load plugin: " << dlerror() << std::endl;
        return -1;
    }

    auto createPlugin = (bsp_dnn::IDnnObjDetectorPlugin* (*)())dlsym(pluginHandle, "create");
    auto destroyPlugin = (void (*)(bsp_dnn::IDnnObjDetectorPlugin*))dlsym(pluginHandle, "destroy");
    
    if (!createPlugin || !destroyPlugin) {
        std::cerr << "Error: Cannot find plugin symbols" << std::endl;
        dlclose(pluginHandle);
        return -1;
    }

    auto plugin = createPlugin();
    std::cout << "Plugin loaded successfully" << std::endl;

    // 步骤2: 创建TensorRT推理引擎
    std::cout << "[2/9] Creating TensorRT engine..." << std::endl;
    auto engine = bsp_dnn::IDnnEngine::create("trt");
    if (!engine) {
        std::cerr << "Error: Failed to create TensorRT engine" << std::endl;
        destroyPlugin(plugin);
        dlclose(pluginHandle);
        return -1;
    }

    try {
        engine->loadModel(modelPath);
        std::cout << "Model loaded successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading model: " << e.what() << std::endl;
        destroyPlugin(plugin);
        dlclose(pluginHandle);
        return -1;
    }

    // 步骤3: 获取模型输入形状
    std::cout << "[3/9] Getting input shape..." << std::endl;
    bsp_dnn::IDnnEngine::dnnInputShape inputShape;
    if (engine->getInputShape(inputShape) != 0) {
        std::cerr << "Error: Failed to get input shape" << std::endl;
        destroyPlugin(plugin);
        dlclose(pluginHandle);
        return -1;
    }
    
    std::cout << "Input shape: " << inputShape.width << "x" << inputShape.height 
              << "x" << inputShape.channel << std::endl;

    // 步骤4: 设置检测参数
    std::cout << "[4/9] Setting detection parameters..." << std::endl;
    bsp_dnn::ObjDetectParams params;
    params.model_input_width = inputShape.width;
    params.model_input_height = inputShape.height;
    params.model_input_channel = inputShape.channel;
    params.conf_threshold = 0.2f;      // 置信度阈值
    params.nms_threshold = 0.5f;       // NMS IoU阈值
    
    std::cout << "Confidence threshold: " << params.conf_threshold << std::endl;
    std::cout << "NMS threshold: " << params.nms_threshold << std::endl;

    // 步骤5: 加载输入图像
    std::cout << "[5/9] Loading input image..." << std::endl;
    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cerr << "Error: Cannot load image: " << imagePath << std::endl;
        destroyPlugin(plugin);
        dlclose(pluginHandle);
        return -1;
    }
    
    std::cout << "Image size: " << image.cols << "x" << image.rows << std::endl;
    auto imagePtr = std::make_shared<cv::Mat>(image);

    // 步骤6: 预处理
    std::cout << "[6/9] Preprocessing..." << std::endl;
    bsp_dnn::ObjDetectInput input;
    input.handleType = "opencv4";
    input.imageHandle = imagePtr;
    
    bsp_dnn::IDnnEngine::dnnInput engineInput;
    if (plugin->preProcess(params, input, engineInput) != 0) {
        std::cerr << "Error: Preprocessing failed" << std::endl;
        destroyPlugin(plugin);
        dlclose(pluginHandle);
        return -1;
    }
    
    std::cout << "Preprocessing complete. Input buffer size: " << engineInput.size << " bytes" << std::endl;

    // 步骤7: 推理
    std::cout << "[7/9] Running inference..." << std::endl;
    auto startTime = cv::getTickCount();
    
    if (engine->pushInputData(engineInput) != 0) {
        std::cerr << "Error: Failed to push input data" << std::endl;
        destroyPlugin(plugin);
        dlclose(pluginHandle);
        return -1;
    }
    
    if (engine->runInference() != 0) {
        std::cerr << "Error: Inference failed" << std::endl;
        destroyPlugin(plugin);
        dlclose(pluginHandle);
        return -1;
    }
    
    std::vector<bsp_dnn::IDnnEngine::dnnOutput> outputs;
    if (engine->popOutputData(outputs) != 0) {
        std::cerr << "Error: Failed to get output data" << std::endl;
        destroyPlugin(plugin);
        dlclose(pluginHandle);
        return -1;
    }
    
    auto endTime = cv::getTickCount();
    double inferenceTime = (endTime - startTime) / cv::getTickFrequency() * 1000.0;
    std::cout << "Inference time: " << inferenceTime << " ms" << std::endl;

    // 步骤8: 后处理
    std::cout << "[8/9] Postprocessing..." << std::endl;
    std::vector<bsp_dnn::ObjDetectOutputBox> detections;
    if (plugin->postProcess(labelPath, params, outputs, detections) != 0) {
        std::cerr << "Error: Postprocessing failed" << std::endl;
        engine->releaseOutputData(outputs);
        destroyPlugin(plugin);
        dlclose(pluginHandle);
        return -1;
    }
    
    std::cout << "Found " << detections.size() << " objects" << std::endl;

    // 步骤9: 可视化和保存结果
    std::cout << "[9/9] Visualizing results..." << std::endl;
    
    // 定义每个类别的颜色 (BGR)
    std::vector<cv::Scalar> colors = {
        cv::Scalar(0, 255, 0),    // Vehicle - 绿色
        cv::Scalar(255, 0, 0),    // TwoWheeler - 蓝色
        cv::Scalar(0, 0, 255),    // Person - 红色
        cv::Scalar(255, 255, 0)   // RoadSign - 青色
    };
    
    for (size_t i = 0; i < detections.size(); i++) {
        const auto& det = detections[i];
        
        // 打印检测信息
        std::cout << "  [" << (i+1) << "] " << det.label 
                  << " (confidence: " << det.score << ")"
                  << " at [" << det.bbox.left << ", " << det.bbox.top 
                  << ", " << det.bbox.right << ", " << det.bbox.bottom << "]" << std::endl;
        
        // 选择颜色
        cv::Scalar color = colors[i % colors.size()];
        
        // 画边界框
        cv::rectangle(image, 
                     cv::Point(det.bbox.left, det.bbox.top),
                     cv::Point(det.bbox.right, det.bbox.bottom),
                     color, 2);
        
        // 添加标签文本
        std::string text = det.label + ": " + std::to_string(det.score).substr(0, 4);
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        
        // 画文本背景
        cv::rectangle(image,
                     cv::Point(det.bbox.left, det.bbox.top - textSize.height - 5),
                     cv::Point(det.bbox.left + textSize.width, det.bbox.top),
                     color, -1);
        
        // 画文本
        cv::putText(image, text, 
                   cv::Point(det.bbox.left, det.bbox.top - 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    }
    
    // 保存结果
    std::string outputPath = "output_" + std::string(imagePath);
    cv::imwrite(outputPath, image);
    std::cout << "Result saved to: " << outputPath << std::endl;
    
    // 可选：显示结果（需要图形界面支持）
    // cv::imshow("Detection Result", image);
    // cv::waitKey(0);

    // 清理
    std::cout << "Cleaning up..." << std::endl;
    engine->releaseOutputData(outputs);
    destroyPlugin(plugin);
    dlclose(pluginHandle);
    
    std::cout << "Done!" << std::endl;
    return 0;
}

