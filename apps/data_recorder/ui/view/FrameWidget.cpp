#include "view/FrameWidget.hpp"

#include "view/FrameImage.hpp"

#include <QPainter>

#include <utility>

namespace apps::data_recorder::ui
{

FrameWidget::FrameWidget(QWidget* parent)
    : QWidget(parent)
{
}

void FrameWidget::setFrame(FramePtr frame)
{
    m_frame = std::move(frame);
    update();
}

void FrameWidget::clearFrame()
{
    m_frame.reset();
    update();
}

void FrameWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (!m_frame)
    {
        return;
    }

    const QImage image = asImageView(*m_frame);
    if (image.isNull())
    {
        return;
    }

    QSize target = image.size();
    target.scale(size(), Qt::KeepAspectRatio);

    const QPoint topLeft((width() - target.width()) / 2, (height() - target.height()) / 2);
    painter.drawImage(QRect(topLeft, target), image);
}

} // namespace apps::data_recorder::ui
