# Video Decode & Encode 测试用例

本用例提供视频解码和编码的性能测试功能。

## 功能说明

### 视频解码 (Decode)
- **输入**: H264、H265 等编码格式的视频文件
- **输出**: YUV 格式的原始视频帧文件
- **支持的解码器**:
  - `rkmpp`: Rockchip MPP 硬件解码器
  - `nvdec`: NVIDIA 硬件解码器 (Jetson 平台)

### 视频编码 (Encode)
- **输入**: YUV 格式的原始视频帧文件
- **输出**: H264、H265 等编码格式的视频文件
- **支持的编码器**:
  - `rkmpp`: Rockchip MPP 硬件编码器
  - `nvenc`: NVIDIA 硬件编码器 (Jetson 平台)

## 编译

在项目根目录执行：
```bash
mkdir -p build && cd build
cmake ..
make video_dec_enc
```

## 使用方法

### 方式一：使用配置文件

#### 解码
```bash
./bin/video_dec_enc --cfg ../bsp_cases/video_dec_enc/decode.ini
```

#### 编码
```bash
./bin/video_dec_enc --cfg ../bsp_cases/video_dec_enc/encode.ini
```

### 方式二：使用命令行参数

#### 解码示例
```bash
./bin/video_dec_enc \
    --model decode \
    --input input.h264 \
    --output output.yuv \
    --decoder rkmpp \
    --encoding h264 \
    --fps 30
```

#### 编码示例
```bash
./bin/video_dec_enc \
    --model encode \
    --input input.yuv \
    --output output.h264 \
    --encoder rkmpp \
    --encodingType h264 \
    --frameFormat YUV420SP \
    --fps 30 \
    --width 1920 \
    --height 1080
```

## 配置参数说明

### 解码参数
- `--model`: 运行模式，设置为 `decode`
- `--input/--inputVideoPath`: 输入视频文件路径
- `--output/--outputYuvPath`: 输出 YUV 文件路径
- `--decoder/--decoderImpl`: 解码器实现 (rkmpp | nvdec)
- `--encoding`: 编码类型 (h264 | h265 | vp8 | vp9 | mpeg2 | mpeg4)
- `--fps`: 帧率

### 编码参数
- `--model`: 运行模式，设置为 `encode`
- `--input/--inputYuvPath`: 输入 YUV 文件路径
- `--output/--outputVideoPath`: 输出视频文件路径
- `--encoder/--encoderImpl`: 编码器实现 (rkmpp | nvenc)
- `--encodingType`: 编码类型 (h264 | h265)
- `--frameFormat`: 帧格式 (默认: YUV420SP)
- `--fps`: 帧率
- `--width`: 视频宽度
- `--height`: 视频高度

## 完整流程示例

1. **解码视频**
```bash
./bin/video_dec_enc \
    --model decode \
    --input test.h264 \
    --output decoded.yuv \
    --decoder rkmpp \
    --encoding h264 \
    --fps 30
```

2. **编码视频**
```bash
./bin/video_dec_enc \
    --model encode \
    --input decoded.yuv \
    --output encoded.h264 \
    --encoder rkmpp \
    --encodingType h264 \
    --fps 30 \
    --width 1920 \
    --height 1080
```

## 注意事项

1. 确保输入文件路径正确且文件可访问
2. 编码时需要指定正确的视频分辨率（width 和 height）
3. YUV 文件没有内嵌的尺寸信息，需要手动指定
4. 不同平台使用对应的解码器/编码器实现（Rockchip 用 rkmpp，Jetson 用 nvdec/nvenc）

