#include "BspTrace.hpp"
#include <fstream>

#include <fcntl.h>
#include <unistd.h>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();
namespace bsp_perf {
namespace common {

BspTrace::BspTrace(const std::string& trace_file_path, const std::string& category_name):
    m_category_name(category_name),
    m_trace_file_path(trace_file_path),
    m_tracingSession(nullptr)
{
    perfetto::TracingInitArgs args;
    args.backends |= perfetto::kInProcessBackend;
    perfetto::Tracing::Initialize(args);
    perfetto::TrackEvent::Register();

    perfetto::TraceConfig cfg;
    cfg.add_buffers()->set_size_kb(10240);
    cfg.add_data_sources()->mutable_config()->set_name("track_event");
    m_trace_file_fd = open(m_trace_file_path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);

    m_tracingSession = perfetto::Tracing::NewTrace();
    m_tracingSession->Setup(cfg, m_trace_file_fd);
    m_tracingSession->StartBlocking();

}

BspTrace::~BspTrace()
{
    m_tracingSession->StopBlocking();
    close(m_trace_file_fd);
    m_tracingSession.reset();
}

} // namespace common
} // namespace bsp_perf
