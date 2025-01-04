#include <bsp_g2d/IGraphics2D.hpp>
#include "rk_rga/rkrga.hpp"
#include <stdexcept>

namespace bsp_g2d
{
std::unique_ptr<IGraphics2D> IGraphics2D::create(const std::string& g2dPlatform)
{
    if (g2dPlatform.compare("rkrga") == 0)
    {
        return std::make_unique<rkrga>();
    }
    else
    {
        throw std::invalid_argument("Invalid G2D platform specified.");
    }
}

}