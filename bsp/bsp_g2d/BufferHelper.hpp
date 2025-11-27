#ifndef __BUFFER_HELPER_HPP__
#define __BUFFER_HELPER_HPP__

#include "IGraphics2D.hpp"

namespace bsp_g2d
{
// ========== Helper RAII Classes ==========

/**
 * @brief RAII type class for mapping and unmapping a buffer
 * @param g2d IGraphics2D instance
 * @param buffer Mapped buffer
 * @param access_mode Access mode: "readwrite", "read", "write"
 */
class BufferMapper
{
public:
    BufferMapper(IGraphics2D* g2d,
                 std::shared_ptr<IGraphics2D::G2DBuffer> buffer,
                 const std::string& access_mode = "readwrite")
        : m_g2d(g2d), m_buffer(buffer)
    {
        if (m_g2d && m_buffer) {
            m_data = m_g2d->mapBuffer(m_buffer, access_mode);
        }
    }

    ~BufferMapper()
    {
        if (m_g2d && m_buffer && m_data) {
            m_g2d->unmapBuffer(m_buffer);
        }
    }

    void* data() { return m_data; }
    explicit operator bool() const { return m_data != nullptr; }

    // Disable copy
    BufferMapper(const BufferMapper&) = delete;
    BufferMapper& operator=(const BufferMapper&) = delete;

private:
    IGraphics2D* m_g2d;
    std::shared_ptr<IGraphics2D::G2DBuffer> m_buffer;
    void* m_data{nullptr};
};

/**
 * @brief RAII guard class for managing buffer synchronization
 * 
 * Similar to std::lock_guard, this class automatically synchronizes buffer data
 * between CPU and Device memory when entering/exiting scope.
 * 
 * @param g2d IGraphics2D instance
 * @param buffer Mapped buffer
 * @param direction Sync direction: CpuToDevice, DeviceToCpu, Bidirectional
 */
class BufferSyncGuard
{
public:
    BufferSyncGuard(IGraphics2D* g2d,
                    std::shared_ptr<IGraphics2D::G2DBuffer> buffer,
                    IGraphics2D::SyncDirection direction)
        : m_g2d(g2d), m_buffer(buffer), m_direction(direction)
    {
        if (m_g2d && m_buffer) {
            if (m_direction == IGraphics2D::SyncDirection::DeviceToCpu ||
                m_direction == IGraphics2D::SyncDirection::Bidirectional) {
                m_g2d->syncBuffer(m_buffer, IGraphics2D::SyncDirection::DeviceToCpu);
            }
        }
    }

    ~BufferSyncGuard()
    {
        if (m_g2d && m_buffer) {
            if (m_direction == IGraphics2D::SyncDirection::CpuToDevice ||
                m_direction == IGraphics2D::SyncDirection::Bidirectional) {
                m_g2d->syncBuffer(m_buffer, IGraphics2D::SyncDirection::CpuToDevice);
            }
        }
    }

    // Disable copy
    BufferSyncGuard(const BufferSyncGuard&) = delete;
    BufferSyncGuard& operator=(const BufferSyncGuard&) = delete;

private:
    IGraphics2D* m_g2d;
    std::shared_ptr<IGraphics2D::G2DBuffer> m_buffer;
    IGraphics2D::SyncDirection m_direction;
};

} // namespace bsp_g2d

#endif // __BUFFER_HELPER_HPP__