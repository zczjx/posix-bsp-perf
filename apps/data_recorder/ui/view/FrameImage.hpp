#ifndef APPS_DATA_RECORDER_UI_VIEW_FRAMEIMAGE_HPP
#define APPS_DATA_RECORDER_UI_VIEW_FRAMEIMAGE_HPP

#include "core/Frame.hpp"

#include <QImage>

namespace apps::data_recorder::ui
{

/**
 * @brief Wraps a frame's pixels in a QImage without copying them.
 *
 * The returned image shares the frame's buffer, so it must not outlive the
 * FramePtr it was created from.
 */
QImage asImageView(const Frame& frame);

/**
 * @brief Copies a QImage into a pooled frame, converting to @p format.
 *
 * Handles the scanline padding QImage may carry, which a plain block copy of
 * QImage::bits() would get wrong for widths that are not 4-byte aligned.
 *
 * @return the frame, or nullptr when @p image cannot be represented.
 */
std::shared_ptr<Frame> frameFromImage(const QImage& image, PixelFormat format, FramePool& pool);

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_VIEW_FRAMEIMAGE_HPP
