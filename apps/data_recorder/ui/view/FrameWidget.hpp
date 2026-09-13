#ifndef APPS_DATA_RECORDER_UI_VIEW_FRAMEWIDGET_HPP
#define APPS_DATA_RECORDER_UI_VIEW_FRAMEWIDGET_HPP

#include "core/Frame.hpp"

#include <QWidget>

namespace apps::data_recorder::ui
{

/**
 * @brief Draws a single frame, letterboxed and centred.
 *
 * Holding the FramePtr keeps the pixels alive for as long as they are on
 * screen, so painting needs no copy and no lock: every caller reaches this
 * widget on the GUI thread, because frames arrive through queued connections.
 */
class FrameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FrameWidget(QWidget* parent = nullptr);

    void setFrame(FramePtr frame);
    void clearFrame();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    FramePtr m_frame;
};

} // namespace apps::data_recorder::ui

#endif // APPS_DATA_RECORDER_UI_VIEW_FRAMEWIDGET_HPP
