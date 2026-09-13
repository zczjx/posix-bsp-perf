#include "view/FrameGridView.hpp"

#include "core/Logging.hpp"
#include "view/FrameImage.hpp"
#include "view/FrameWidget.hpp"

#include <QGridLayout>
#include <QLayoutItem>
#include <QPixmap>

#include <algorithm>
#include <utility>

namespace apps::data_recorder::ui
{

namespace
{

constexpr int kMinimumCellWidth = 160;
constexpr int kMinimumCellHeight = 120;

} // namespace

FrameGridView::FrameGridView(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QGridLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(2);
    setStyleSheet("background-color: black;");
}

void FrameGridView::configure(const GridSize& grid, const std::vector<StreamPlacement>& placements)
{
    while (QLayoutItem* item = m_layout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
        {
            widget->deleteLater();
        }

        delete item;
    }

    m_cells.clear();
    m_cellByName.clear();

    const int rows = std::max(1, grid.rows);
    const int columns = std::max(1, grid.columns);
    m_cells.reserve(static_cast<std::size_t>(rows) * columns);

    for (int row = 0; row < rows; ++row)
    {
        m_layout->setRowStretch(row, 1);
        for (int column = 0; column < columns; ++column)
        {
            auto* cell = new FrameWidget(this);
            cell->setMinimumSize(kMinimumCellWidth, kMinimumCellHeight);
            m_layout->addWidget(cell, row, column);
            m_layout->setColumnStretch(column, 1);
            m_cells.push_back(cell);
        }
    }

    for (const auto& placement: placements)
    {
        if (placement.row >= rows || placement.column >= columns)
        {
            continue;
        }

        m_cellByName[placement.name] =
            m_cells[static_cast<std::size_t>(placement.row) * columns + placement.column];
    }
}

void FrameGridView::setFrame(const std::string& sourceName, FramePtr frame)
{
    auto it = m_cellByName.find(sourceName);
    if (it == m_cellByName.end())
    {
        return;
    }

    it->second->setFrame(std::move(frame));
}

void FrameGridView::clearFrames()
{
    for (auto* cell: m_cells)
    {
        cell->clearFrame();
    }
}

std::shared_ptr<Frame> FrameGridView::grabComposite(const QSize& size, PixelFormat format)
{
    QImage image = grab().toImage();
    if (image.isNull())
    {
        return nullptr;
    }

    // Encoders reject odd dimensions, so snap the request down to even numbers.
    const QSize target(size.width() & ~1, size.height() & ~1);
    if (target.width() <= 0 || target.height() <= 0)
    {
        return nullptr;
    }

    if (image.size() != target)
    {
        image = image.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    return frameFromImage(image, format, m_compositePool);
}

} // namespace apps::data_recorder::ui
