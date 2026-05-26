#include "ISurroundView.hpp"
#include "SurroundView.hpp"
#include <stdexcept>

namespace bsp_perf
{
namespace svs
{

std::unique_ptr<ISurroundView> ISurroundView::create(const std::string& backend)
{
    if (backend == "opencv") {
        return std::make_unique<OpenCvSurroundView>();
    }

    throw std::invalid_argument("unsupported surround view backend: " + backend);
}

} // namespace svs
} // namespace bsp_perf
