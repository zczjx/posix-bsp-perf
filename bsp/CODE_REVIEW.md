# BSP 目录架构分析与代码评审

## 整体定位

跨平台 BSP (Board Support Package) 硬件抽象层，统一 **Rockchip RK35xx** 和 **NVIDIA Jetson** 两大嵌入式平台的多媒体与 AI 推理操作。

典型数据流：

```
bsp_container(解封装) -> bsp_codec(解码) -> bsp_g2d(缩放/色彩转换)
    -> bsp_dnn(推理) -> bsp_egl(渲染显示)
    -> bsp_sockets + protocol(网络传输)
    -> profiler(性能度量)
    -> framework(测试用例生命周期)
```

依赖关系：

```
shared (bsp_shared)
  |
  +-- framework (case_framework) --> shared
  +-- profiler (bsp_profiler) --> shared, perfetto
  +-- protocol (bsp_protocol) --> [standalone]
  +-- bsp_sockets --> shared
  +-- bsp_container --> shared, FFmpeg libs
  +-- bsp_codec (bsp_enc, bsp_dec) --> shared, platform codec libs
  +-- bsp_g2d --> shared, platform 2D libs
  +-- bsp_dnn (dnnObjDetector) --> shared, platform DNN libs, msgpack
  +-- bsp_egl --> EGL, GLESv2, X11
```

---

## 做得好的部分

### 1. 接口抽象 + 工厂模式

所有硬件相关模块 (codec/g2d/dnn/sockets) 都遵循 `Interface + create(platform)` 模式，上层代码不感知底层平台差异。设计一致性很高。

### 2. bsp_g2d 的 BufferHelper RAII 设计

`BufferMapper` (自动 map/unmap) 和 `BufferSyncGuard` (自动 CPU/Device 同步) 参考了 `std::lock_guard` 的思路，在异构内存管理场景下很实用。`queryCapability()` 运行时能力查询也是好的设计。

### 3. DNN 的 Plugin 架构

`dlopen/dlsym` + `CREATE_PLUGIN_INSTANCE` 宏实现热插拔检测器插件，无需重编译。解耦了推理引擎和后处理逻辑。

### 4. bsp_sockets 模块功能完整

提供了 epoll/poll/libevent 三种事件循环实现 + TCP/UDP Server/Client + 线程池 + 定时器队列 + 消息分发，是一个功能齐全的网络库。

### 5. Template Method 模式

`BasePerfCase::run()` 定义了 `onInit -> onProcess/onRender loop -> onRelease` 生命周期骨架，子类只实现四个虚函数，结构清晰。

### 6. 命名空间组织

`bsp_perf::common`, `bsp_codec`, `bsp_dnn`, `bsp_g2d` 等命名空间划分清晰。

---

## 需要改进的部分

### 一、严重 Bug / 未定义行为

| 问题 | 位置 | 说明 |
|------|------|------|
| Division by zero | `rkmppDec.cpp:277` | `fps` 默认为 0，`1000/fps` 直接崩溃 |
| dlclose 双重释放 | `dnnObjDetector.cpp:61-62` | `shared_ptr` deleter 和手动 `dlclose` 各调一次 |
| Plugin 未通过 destroy() 销毁 | `dnnObjDetector.cpp:34,56` | `shared_ptr` 用默认 `delete`，但对象由 .so 内部 `new` 创建，堆不匹配 |
| 析构函数中 throw | `dnnObjDetector.cpp:52` | 栈展开期间触发 `std::terminate` |
| `std::sort` 比较器违反严格弱序 | `YoloPostProcess.cpp:81` | `>=` 导致 UB，可能无限循环 |
| rkmppEnc 溢出检查用错变量 | `rkmppEnc.cpp:481` | 检查 `log_len` 而非实际数据 `len`，`memcpy` 越界 |
| rkmppEnc RGB 帧大小计算错误 | `rkmppEnc.cpp:67-82` | RGB888 是 3 bytes/pixel，但未乘以系数 |
| nvVideoDec DMA 缓冲区泄漏 | `nvVideoDec.cpp:54-57` | 析构时只清零 fd，从未 close/destroy |
| `runAfter()` 缺少 return | 三个 EventLoop 实现 | 返回垃圾 timer ID，定时器无法取消 |
| `FFmpegMuxer` 两个函数缺 return | `FFmpegMuxer.cpp:17,262` | `openContainerMux` 和 `endStreamMux` 无返回值 |
| FFmpegStreamReader double-free | `FFmpegStreamReader.cpp:13-32` | 错误路径手动 `avformat_close_input` + shared_ptr deleter 再调一次 |
| AVCodecParameters 浅拷贝 | `FFmpegStreamReader.cpp:45-51` | `memcpy` 导致 `extradata` 指针悬空 |
| NvVic 格式映射错误 | `NvVicGraphics2D.cpp:48-70` | BGRA->RGBA、YUYV->UYVY、422->420 全部映射错误，产出画面色彩错乱 |
| RTP 解析有符号溢出 | `RtpReader.cpp:25-27` | `uint8_t << 24` 在 signed int 上是 UB |
| `BspFileUtils` double-munmap | `BspFileUtils.cpp:36,49` | shared_ptr deleter + `ReleaseFileMmap` 各调一次 munmap |
| rkmppEnc 析构顺序错误 | `rkmppEnc.cpp:11-43` | 先 `mpp_destroy(ctx)` 再释放 buffer，应反过来 |
| nvVideoDec 回调悬空指针 | `nvVideoDec.cpp:328,444-491` | `m_frame_buffer` 是局部变量，回调后 `virt_addr` 悬空 |
| rkmppDec 回调悬空指针 | `rkmppDec.cpp:263-289` | MPP frame 释放后 `DecodeOutFrame::virt_addr` 悬空 |
| RKNN 固定输入数组大小 | `rknn.hpp:28` | `rknn_input m_inputs[1]` 硬编码，多输入模型溢出 |
| TensorRT 固定 I/O 数组 | `tensorrt.hpp:41-45` | `m_deviceBuffers[10]`, `m_outputIndices[8]` 无越界检查 |
| cudaMalloc/malloc 未检查返回值 | `tensorrt.cpp:217-229` | GPU 内存分配失败后续 cudaMemcpy 崩溃 |

### 二、资源泄漏 / 竞态条件

| 问题 | 位置 |
|------|------|
| epoll fd 从未 close | `EventLoopEpoll.cpp` — 无析构函数 |
| libevent `event_base` 从未释放 | `EventLoopLibevent.cpp` — 无析构函数 |
| libevent timer event 局部变量泄漏 | `EventLoopLibevent.cpp:99-109` |
| libevent `delIoEvent` 只释放 read 不释放 write | `EventLoopLibevent.cpp:225-248` — `if/else if` 逻辑 |
| libevent `addIoEvent` 重注册时旧 event 泄漏 | `EventLoopLibevent.cpp:131-182` |
| libevent `m_libevent_io_events` entry 从不 erase | `EventLoopLibevent.cpp:184-248` |
| 事件循环无退出机制 | 三个实现都是 `while(true)` 无 `m_running` 标志 |
| `ThreadPool` detach 线程，无法 join | `ThreadPool.cpp:95` — 析构后线程继续跑，悬空引用 |
| `threadDomain` 缺少 return 语句 | `ThreadPool.cpp:61-68` — 返回类型 `std::any` 但无 return |
| Event loop 内部数据结构无锁 | `addTask()` 与 `runTask()` 竞态 |
| `TcpServer::doAccept` 连接池不加锁 | `TcpServer.cpp:243` |
| `TcpServer::doAccept` TOCTOU 竞态 | `TcpServer.cpp:212` — getConnectionNum 和 incConnection 之间 |
| UDP `sendData` 用 `insert` 破坏 `m_wbuf` | `UdpServer.cpp:148` / `UdpClient.cpp:145` |
| `OutputBufferQueue::writeFd` 不处理 partial write | `IOBuffer.cpp:57-79` — 数据丢失 |
| `IOBufferQueue::getFrontBuffer` 返回无锁引用 | `IOBuffer.hpp:52-56` |
| TcpClient handleRead 假设消息原子到达 | `TcpClient.cpp:219-283` — TCP 是流协议，缺少重组 |
| `TcpConnection::handleRead` 不验证 buffer 大小 | `TcpConnection.cpp:88` — memcpy 可能越界 |
| `BspLogger` 析构调全局 `spdlog::shutdown()` | `BspLogger.cpp:43` — 杀死所有 logger |
| `std::localtime` 线程不安全 | `BspTimeUtils.hpp:20` |
| `LoadFileMmap` mmap 失败时泄漏 fd | `BspFileUtils.cpp:28-33` |
| `LoadFileMalloc` 成功后不关闭 fd | `BspFileUtils.cpp:59-88` |
| `LoadFileMalloc` 不处理 partial read | `BspFileUtils.cpp:81` |
| NvVic `createBuffer` Mapped 失败泄漏 NvBufSurface | `NvVicGraphics2D.cpp:266-280` |
| rkmppDec `setup()` 失败不清理 mpp_ctx | `rkmppDec.cpp:58-106` |

### 三、架构 / 设计问题

#### 1. `IEventLoop` 接口耦合 epoll 常量

`IEventLoop.hpp` 包含 `<sys/epoll.h>` 和 `<poll.h>`，`EventLoopParams` 里同时有三个后端的字段。Poll 和 libevent 实现被迫翻译 `EPOLLIN/EPOLLOUT`。应该定义自己的 IO 事件枚举。

#### 2. `std::any` 滥用

`G2DBuffer::opaque_handle`、`EncodeInputBuffer::internal_buf` 都用 `std::any`，丢失了类型安全。跨平台误传 buffer 只会在运行时 `bad_any_cast`，无编译期保护。应该用 `variant` 或平台特化的派生类型。

#### 3. POD 结构体成员未初始化

`DecodePacket::pkt_eos`、`DecodeOutFrame::fd/eos_flag`、`EncodeConfig::width/height/hor_stride/ver_stride`、`StreamCodecParams` 多个字段、`StreamPacket` 多个字段都没有默认值，容易出垃圾值 bug。

#### 4. 预处理器 `#ifdef` 工厂 + `else` 结合有逻辑缺陷

```cpp
#ifdef PLATFORM_A
    if (...) { ... }
#endif
#ifdef PLATFORM_B
    if (...) { ... }
#endif
    else { throw ...; }  // 只绑定到最后一个 if
```

当只定义 PLATFORM_A 时，无效平台名不会抛异常。应重构为 `if/else if/else` 链或注册表模式。

#### 5. 大量 `std::cout` 残留在热路径

codec 的每帧解码/编码、NvVic 的 buffer 操作、FFmpeg 的每包处理都有 `cout/cerr` 输出（包含 `"zczjx-->"` 调试前缀）。对于性能测试框架来说，stdio 锁会严重拖慢吞吐。

#### 6. 错误处理不一致

bsp_sockets 用 BspLogger，bsp_container 用裸 `cerr`，codec/dnn 混用 cout 和 logger。应统一为 BspLogger。

#### 7. DNN Plugin 宏放在 .hpp 而非 .cpp

`CREATE_PLUGIN_INSTANCE` 定义 `extern "C"` 函数，放在头文件会导致多 TU 包含时链接重复定义。涉及 4 个插件头文件：
- `rknnYolov5.hpp:26`
- `RGArknnYolov5.hpp:36`
- `Resnet18TrafficCamNet.hpp:121`
- `NvVicResnet18TrafficCamNet.hpp:135`

#### 8. RTP 解析器功能不完整

- 忽略 CSRC 和扩展头，假设 payload 紧跟固定 12 字节头部
- `RtpHeader` 结构与 RFC 3550 不匹配（version/padding/extension/CC 合并为一个字节）
- `const_cast` 破坏 const 正确性 (`RtpReader.cpp:40`)
- `RtpReader` 无状态却有虚析构函数，增加不必要的 vtable 开销

#### 9. 大量代码重复

- `NvVicResnet18TrafficCamNet` vs `Resnet18TrafficCamNet` — ~200 行重复 (postProcess/NMS/parseDetectNet)
- RGA core affinity WAR 在 `rkrga.cpp` 三个函数中 copy-paste
- 三个 EventLoop 的 `runAfter`、`processEvents`、`runTask` 逻辑几乎相同

#### 10. BasePerfCase::run() 无异常安全

`onProcess/onRender` 抛异常时 `onRelease()` 不会被调用，资源泄漏。应加 try/catch 或 RAII guard。

#### 11. OpenGL 渲染问题

- `glGetAttribLocation` 返回 GLint (-1=失败) 存入 GLuint，-1 变成巨大正数 (`RenderEGL.cpp:326-328`)
- `renderFramebuffer` 每帧调 `glTexImage2D` 而非 `glTexSubImage2D`，重复分配纹理存储
- ARGB/RGBA 字节序不匹配导致红蓝通道互换
- 渲染循环不处理 X11 事件，窗口关闭按钮失效
- 全文件无 `glGetError()` 调用

#### 12. 其他设计问题

- `rkrga::releaseBuffer` 参数按值传递 shared_ptr，reset 只作用于局部拷贝，函数是 no-op
- `NvVicGraphics2D::mapBuffer` 只映射 plane 0，多平面格式 (NV12) 的 UV 数据不可访问
- `BytesPerPixel` 和 `rgaPixelFormat` 在 const map 上加不必要的 mutex
- `MsgDispatcher::genCmdId` 用 `std::hash<string>`，不抗碰撞，碰撞会覆盖回调
- `EventLoopPoll` 固定 `pollfd[1024]`，超过 1024 fd 溢出
- `TimerQueue::m_pioneer` 用 `uint64_t(-1)` 做哨兵值，可读性差
- `assert(connfd < m_conns_size)` Release 模式下被移除，越界访问
- `ArgParser::parseArgs` 调 `std::exit()` 跳过所有 RAII 清理

### 四、死代码

| 文件 | 说明 |
|------|------|
| `profiler/TimeTagger.hpp/.cpp` | 0 字节空文件 |
| `profiler/TraceEvent.hpp/.cpp` | 0 字节空文件 |
| `nvVideoEnc::getEncoderHeader()` | 返回空数据，实现被注释掉 |
| `PerfProfiler::memValueMB` | 左移 20 位（乘以 1MB）而非右移，语义反了 |
| `BspTrace::m_category_name` | 存储但未使用 |
| `EXTRA_MEM_LIMIT` 常量 | 从未使用 |
| `processEvents` 里 `thread_local tid` | 从未使用 |
| `FFmpegDemuxer::seekStreamFrame` | 空实现返回 0 |

---

## 改进建议优先级

### P0 — 立即修复 (UB / 崩溃 / 数据损坏)

1. rkmppDec fps=0 division-by-zero
2. dnnObjDetector 析构: double-dlclose + plugin 不用 destroy() + 析构中 throw
3. YoloPostProcess `>=` 比较器改为 `>`
4. rkmppEnc encode() 溢出检查变量 `log_len` -> `len`
5. rkmppEnc calculateFrameSize RGB 帧大小乘以 bytes-per-pixel
6. EventLoop `runAfter()` 加 return
7. FFmpegMuxer `openContainerMux`/`endStreamMux` 加 return
8. FFmpegStreamReader 错误路径 double-free: 移除手动 `avformat_close_input`
9. FFmpegStreamReader `memcpy` 改 `avcodec_parameters_copy`
10. NvVic 格式映射修正 (BGRA/YUYV/422)
11. RTP 解析: `static_cast<uint32_t>()` before shift
12. BspFileUtils double-munmap: 移除 `ReleaseFileMmap` 中的 munmap 或改用 raw pointer
13. rkmppEnc 析构: 先释放 buffer 再 destroy context

### P1 — 尽快修复 (资源泄漏 / 竞态)

1. EventLoopEpoll/Libevent 加析构函数释放 fd/event_base
2. 事件循环加 `m_running` 标志和 `stop()` 方法
3. ThreadPool 改 join 不用 detach
4. Event loop `addTask`/`runTask` 加锁
5. UDP sendData 修复 `m_wbuf` insert 逻辑
6. OutputBufferQueue 处理 partial write
7. TcpClient 加消息重组逻辑
8. nvVideoDec 析构释放 DMA buffer
9. BspLogger 析构不调全局 shutdown
10. codec 回调: 拷贝数据而非传指针，或延长 buffer 生命周期

### P2 — 架构改进

1. 统一错误处理为 BspLogger，移除所有 cout/cerr
2. IEventLoop 定义自有事件枚举，解耦 epoll 常量
3. `std::any` 改 variant 或平台特化类型
4. 工厂方法重构为注册表模式
5. POD 结构体全部加默认初始化
6. Plugin 宏移到 .cpp
7. 提取重复代码 (Resnet18 插件、RGA WAR、EventLoop 公共逻辑)
8. BasePerfCase::run() 加 try/catch 保证 onRelease
9. RenderEGL 修复 ARGB/RGBA 字节序 + 用 glTexSubImage2D
10. RTP 解析器支持 CSRC/扩展头

### P3 — 清理

1. 删除空文件 (TimeTagger, TraceEvent)
2. 删除死代码 (EXTRA_MEM_LIMIT, thread_local tid, memValueMB)
3. 修复保留标识符 `__NAME__` include guard
4. 实现 nvVideoEnc::getEncoderHeader 或标记为不支持
5. 实现 FFmpegDemuxer::seekStreamFrame 或标记为不支持
