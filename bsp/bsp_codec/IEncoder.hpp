#ifndef __IENCODER_HPP__
#define __IENCODER_HPP__

#include <memory>
#include <string>
#include <any>
#include <functional>

namespace bsp_codec
{
using encodeReadyCallback = std::function<void(std::any userdata, const char* data, int size)>;
struct EncodeConfig
{
    // encoding can be "h264", "h265"
    std::string encodingType{"h264"};
    std::string frameFormat{"YUV420SP"};
    int fps{0};
    uint32_t width;
    uint32_t height;
    uint32_t hor_stride;
    uint32_t ver_stride;
};

struct EncodePacket
{
    size_t max_size{0};
    int pkt_eos;
    size_t pkt_len{0};
    std::vector<uint8_t> encode_pkt{};
};

struct EncodeInputBuffer
{
    std::any internal_buf{nullptr};
    int input_buf_fd{-1};
    void* input_buf_addr{nullptr};
};

class IEncoder
{
public:
    /**
     * @brief Factory function to create an IEncoder instance.
     *
     * This static method creates and returns a unique pointer to an IEncoder
     * instance based on the specified codec platform.
     *
     * @param codecPlatform A string representing the codec platform for which
     * the IEncoder instance is to be created.
     * Supported values:
     * - "rkmpp" for Rockchip MPP encoder
     * - "nvenc" for NVIDIA NVENC encoder (Jetson platforms)
     *
     * @throws std::invalid_argument If an invalid codec platform is specified.
     * @return std::unique_ptr<IEncoder> A unique pointer to the created IEncoder instance.
     */
    static std::unique_ptr<IEncoder> create(const std::string& codecPlatform);

    virtual int setup(EncodeConfig& cfg) = 0;
    virtual void setEncodeReadyCallback(encodeReadyCallback callback, std::any userdata) = 0;

    /**
     * @brief Get an input buffer for encoding.
     * 
     * This method returns a pre-allocated input buffer managed by the encoder.
     * The buffer is owned by the encoder and will be automatically returned to
     * the buffer pool after encode() is called.
     * 
     * Memory Management:
     * - The encoder allocates and manages the buffer memory
     * - Both rkmpp and nvenc implementations provide allocated buffers
     * - The buffer's input_buf_addr is ready to be written to
     * - User should write frame data directly to input_buf_addr
     * - Buffer is automatically released back to pool after encode()
     * 
     * Usage:
     *   auto buf = encoder->getInputBuffer();
     *   memcpy(buf->input_buf_addr, frame_data, frame_size);
     *   encoder->encode(*buf, out_pkt);
     *   // Buffer automatically returned to pool
     * 
     * @return std::shared_ptr<EncodeInputBuffer> A shared pointer to the input buffer,
     *         or nullptr if no buffer is available (pool exhausted)
     */
    virtual std::shared_ptr<EncodeInputBuffer> getInputBuffer() = 0;
    
    /**
     * @brief Encode a frame and return the encoded data.
     * 
     * This method encodes the input frame data and returns the encoded packet.
     * Both rkmpp and nvenc implementations now work synchronously - the encoded
     * data is available in out_pkt when this function returns.
     * 
     * Behavior:
     * - Reads frame data from input_buf.input_buf_addr
     * - Encodes the frame
     * - Fills out_pkt with encoded data (if out_pkt.encode_pkt has capacity)
     * - Calls the callback (if set) with encoded data
     * - Automatically releases input_buf back to pool
     * 
     * Initial Fill Phase (nvenc):
     * - NVIDIA encoder needs several frames queued before producing output
     * - During initial fill, out_pkt.pkt_len will be 0
     * - After initial fill completes, each encode() returns one frame
     * 
     * Usage:
     *   EncodePacket out_pkt;
     *   out_pkt.max_size = frame_size * 2;
     *   out_pkt.encode_pkt.resize(out_pkt.max_size);
     *   
     *   encoder->encode(*input_buf, out_pkt);
     *   
     *   if (out_pkt.pkt_len > 0) {
     *       // Encoded data available in out_pkt.encode_pkt
     *       write_to_file(out_pkt.encode_pkt.data(), out_pkt.pkt_len);
     *   }
     * 
     * @param input_buf Input buffer obtained from getInputBuffer()
     * @param out_pkt Output packet to be filled with encoded data
     * @return 0 on success, negative on error
     */
    virtual int encode(EncodeInputBuffer& input_buf, EncodePacket& out_pkt) = 0;
    virtual int getEncoderHeader(std::string& headBuf) = 0;
    virtual int tearDown() = 0;

    virtual size_t getFrameSize() = 0;
    virtual ~IEncoder() = default;

protected:
    IEncoder() = default;
    IEncoder(const IEncoder&) = delete;
    IEncoder& operator=(const IEncoder&) = delete;
};

} // namespace bsp_codec

#endif // __IENCODER_HPP__