#ifndef APPS_DATA_RECORDER_UI_VIEW_FRAMEGRIDVIEW_HPP
#define APPS_DATA_RECORDER_UI_VIEW_FRAMEGRIDVIEW_HPP

#include "config/UiConfig.hpp"
#include "core/Frame.hpp"

#include <QSize>
#include <QWidget>

#include <string>
#include <unordered_map>
#include <vector>

class QGridLayout;

namespace apps::data_recorder::ui
{

class FrameWidget;

/**
 * @brief A grid of FrameWidgets addressed by stream name.
 *
 * The view is told which cell each stream occupies; it does not read
 * configuration files and holds no application state.
 */
class FrameGridView : public QWidget
{
    Q_OBJECT

public:
    explicit FrameGridView(QWidget* parent = nullptr);

    /// Rebuilds the grid and maps each placement's stream name onto a cell.
    void configure(const GridSize& grid, const std::vector<StreamPlacement>& placements);

    /// Shows @p frame in the cell owned by @p sourceName; ignored if unmapped.
    void setFrame(const std::string& sourceName, FramePtr frame);

    void clearFrames();

    /**
     * @brief Renders the grid into a frame suitable for recording.
     *
     * @param size   the output resolution; dimensions are rounded down to even
     *               numbers because the encoders require it.
     * @param format the pixel format the recording pipeline expects.
     * @return the composited frame, or nullptr if nothing could be grabbed.
     */
    std::shared_ptr<Frame> grabComposite(const QSize& size, PixelFormat format);

private:
    QGridLayout* m_layout{nullptr};
    std::vector<FrameWidget*> m_cells;
    std::unordered_map<std::string, FrameWidget*> m_cellByName;
    FramePool m_compositePool{2};
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_VIEW_FRAMEGRIDVIEW_HPP
