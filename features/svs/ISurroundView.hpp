#ifndef __I_SURROUND_VIEW_HPP__
#define __I_SURROUND_VIEW_HPP__

#include "SvsTypes.hpp"
#include <memory>
#include <string>

namespace bsp_perf
{
namespace svs
{

class ISurroundView
{
public:
    /**
     * @brief Create a surround view implementation.
     * @param backend Currently supported: "opencv".
     */
    static std::unique_ptr<ISurroundView> create(const std::string& backend);

    virtual int setup(const SurroundViewConfig& config) = 0;
    virtual int process(const FrameSet& input, OutputFrame& output) = 0;
    virtual int tearDown() = 0;
    virtual ~ISurroundView() = default;

protected:
    ISurroundView() = default;
    ISurroundView(const ISurroundView&) = delete;
    ISurroundView& operator=(const ISurroundView&) = delete;
};

} // namespace svs
} // namespace bsp_perf

#endif // __I_SURROUND_VIEW_HPP__
