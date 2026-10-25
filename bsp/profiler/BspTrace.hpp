#ifndef __BSP_TRACE_HPP__
#define __BSP_TRACE_HPP__

#include <perfetto.h>
#include <string>
#include <memory>

#define BSP_CATEGORY_NAME "BspTrace"

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category(BSP_CATEGORY_NAME).SetDescription("BSP Trace"));

#define BSP_TRACE_EVENT_BEGIN(tag_name) \
    TRACE_EVENT_BEGIN(BSP_CATEGORY_NAME, tag_name)

#define BSP_TRACE_EVENT_END() \
    TRACE_EVENT_END(BSP_CATEGORY_NAME)

// RAII 作用域 tag：离开作用域（含 return / 异常）时自动结束，不会出现 BEGIN/END 不配对。
// tag_name 必须是字符串字面量；同一作用域内每行最多用一次。
#define BSP_TRACE_SCOPE(tag_name) \
    TRACE_EVENT(BSP_CATEGORY_NAME, tag_name)

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