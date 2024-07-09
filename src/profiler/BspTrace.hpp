#ifndef __BSP_TRACE_HPP__
#define __BSP_TRACE_HPP__

#include "perfetto.h"
#include <string>
#include <memory>

#define BSP_CATEGORY_NAME "BspTrace"

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category(BSP_CATEGORY_NAME).SetDescription("BSP Trace"));

#define BSP_TRACE_EVENT_BEGIN(tag_name) \
    TRACE_EVENT_BEGIN(BSP_CATEGORY_NAME, tag_name)

#define BSP_TRACE_EVENT_END() \
    TRACE_EVENT_END(BSP_CATEGORY_NAME)

namespace bsp_perf {
namespace common {

class BspTrace {

public:
    BspTrace(const std::string& trace_file_path, const std::string& category_name = BSP_CATEGORY_NAME);
    ~BspTrace();

    BspTrace(const BspTrace&) = delete;
    BspTrace& operator=(const BspTrace&) = delete;

    BspTrace(BspTrace&&) = delete;
    BspTrace& operator=(BspTrace&&) = delete;

private:
    std::string m_category_name;
    std::string m_trace_file_path;
    int m_trace_file_fd{-1};
    std::unique_ptr<perfetto::TracingSession> m_tracingSession;
};

} // namespace common
} // namespace bsp_perf

#endif // __BSP_TRACE_HPP__