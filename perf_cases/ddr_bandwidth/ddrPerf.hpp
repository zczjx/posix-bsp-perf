/*
MIT License

Copyright (c) 2024 Clarence Zhou<287334895@qq.com> and contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#ifndef DDR_PERF_HPP
#define DDR_PERF_HPP

#include <framework/BasePerfCase.hpp>
#include <shared/ArgParser.hpp>
#include <profiler/PerfProfiler.hpp>
#include <memory>
#include <string>
#include <cstring>
#include <iostream>
#include <cstddef>
#include <cstdlib>
#include <ctime>

namespace bsp_perf {
namespace perf_cases {

constexpr int32_t num_1M = 1000000;

class ddrPerf : public bsp_perf::common::BasePerfCase
{

public:
    static constexpr char LOG_TAG[] {"[ddrPerf]: "};

    ddrPerf(bsp_perf::shared::ArgParser&& args):
        BasePerfCase(std::move(args))
    {
        auto& params = getArgs();
        std::string ret;
        params.getOptionVal("--case_name", ret);
        std::string file_path;
        params.getOptionVal("--profile_path", file_path);
        m_profiler = std::make_unique<bsp_perf::common::PerfProfiler>(ret, file_path);

    }
    ddrPerf(const ddrPerf&) = delete;
    ddrPerf& operator=(const ddrPerf&) = delete;
    ddrPerf(ddrPerf&&) = delete;
    ddrPerf& operator=(ddrPerf&&) = delete;
    ~ddrPerf()
    {
        // Release any resources held by m_profiler
        m_profiler.reset();
    }
private:

    void onInit() override
    {
        auto& params = getArgs();
        size_t mem_size;
        params.getOptionVal("--size_mb", mem_size);
        m_bytes_cnt = bsp_perf::common::memValueMB(mem_size);
        m_test_arr.reset(new uint32_t[m_bytes_cnt / sizeof(uint32_t)]);
        m_rw_cnt = m_bytes_cnt / sizeof(uint32_t);
        srand(time(0));
        for(size_t i = 0; i < m_rw_cnt; i++)
        {
            m_test_arr[i] = rand();
        }

    }

    void onProcess() override
    {
        {
            auto begin = m_profiler->getCurrentTimePoint();
            rawPtrWritingPerf(m_test_arr.get(), m_rw_cnt);
            auto end = m_profiler->getCurrentTimePoint();
            auto raw_ptr_write_latency = m_profiler->getLatencyUs(begin, end);
            m_raw_write_bw_mb = getDDRBandwidthMB(raw_ptr_write_latency);
            m_profiler->asyncRecordPerfData("Raw PTR Writing BandWidth", m_raw_write_bw_mb, "MB/s");
        }

        {
            auto begin = m_profiler->getCurrentTimePoint();
            smartPtrWritingPerf(m_test_arr, m_rw_cnt);
            auto end = m_profiler->getCurrentTimePoint();
            auto smart_ptr_write_latency = m_profiler->getLatencyUs(begin, end);
            m_smart_write_bw_mb = getDDRBandwidthMB(smart_ptr_write_latency);
            m_profiler->asyncRecordPerfData("Smart PTR Writing BandWidth", m_smart_write_bw_mb, "MB/s");
        }

        {
            auto begin = m_profiler->getCurrentTimePoint();
            rawPtrReadingPerf(m_test_arr.get(), m_rw_cnt);
            auto end = m_profiler->getCurrentTimePoint();
            auto raw_ptr_read_latency = m_profiler->getLatencyUs(begin, end);
            m_raw_read_bw_mb = getDDRBandwidthMB(raw_ptr_read_latency);
            m_profiler->asyncRecordPerfData("Raw PTR Reading BandWidth", m_raw_read_bw_mb, "MB/s");
        }

        {
            auto begin = m_profiler->getCurrentTimePoint();
            smartPtrReadingPerf(m_test_arr, m_rw_cnt);
            auto end = m_profiler->getCurrentTimePoint();
            auto smart_ptr_read_latency = m_profiler->getLatencyUs(begin, end);
            m_smart_read_bw_mb = getDDRBandwidthMB(smart_ptr_read_latency);
            m_profiler->asyncRecordPerfData("Smart PTR Reading BandWidth", m_smart_read_bw_mb, "MB/s");
        }

        {
            std::unique_ptr<uint32_t[]> dst_ptr = std::make_unique<uint32_t[]>(m_bytes_cnt / sizeof(uint32_t));
            auto begin = m_profiler->getCurrentTimePoint();
            stdMemcpyPerf(m_test_arr.get(), dst_ptr.get(), m_bytes_cnt);
            auto end = m_profiler->getCurrentTimePoint();
            auto raw_ptr_memcpy_latency = m_profiler->getLatencyUs(begin, end);
            m_memcpy_bw_mb = getDDRBandwidthMB(raw_ptr_memcpy_latency);
            m_profiler->asyncRecordPerfData("Raw PTR Memcpy BandWidth", m_memcpy_bw_mb, "MB/s");
        }

    }

    void onRender() override
    {
        m_profiler->printPerfData("Raw PTR Writing BandWidth", m_raw_write_bw_mb, "MB/s");
        m_profiler->printPerfData("Smart PTR Writing BandWidth", m_smart_write_bw_mb, "MB/s");
        m_profiler->printPerfData("Raw PTR Reading BandWidth", m_raw_read_bw_mb, "MB/s");
        m_profiler->printPerfData("Smart PTR Reading BandWidth", m_smart_read_bw_mb, "MB/s");
        m_profiler->printPerfData("Raw PTR Memcpy BandWidth", m_memcpy_bw_mb, "MB/s");
    }

    void onRelease() override
    {
        m_test_arr.reset();
    }

    inline void stdMemcpyPerf(uint32_t* src, uint32_t* dst, size_t bytes_cnt = 0)
    {
        std::memcpy(dst, src, bytes_cnt);
    }

    inline void rawPtrWritingPerf(uint32_t* ptr, size_t cnt = 0)
    {
        for(size_t i = 0; i < cnt; i++)
        {
            ptr[i] = 0x55;
        }
    }

    inline void smartPtrWritingPerf(std::unique_ptr<uint32_t[]>& ptr, size_t cnt = 0)
    {
        for(size_t i = 0; i < cnt; i++)
        {
            ptr[i] = 0xaa;
        }
    }

    inline void rawPtrReadingPerf(uint32_t* ptr, size_t cnt = 0)
    {
        size_t sum = 0;
        for(size_t i = 0; i < cnt; i++)
        {
            sum += ptr[i];
        }
    }

    inline void smartPtrReadingPerf(std::unique_ptr<uint32_t[]>& ptr, size_t cnt = 0)
    {
        size_t sum = 0;
        for(size_t i = 0; i < cnt; i++)
        {
            sum += ptr[i];
        }
    }

    inline float getDDRBandwidthMB(const size_t& test_time_us)
    {
        float bytes_per_us = 0.0;
	    float bytes_per_sec = 0.0;
	    float mb_per_sec = 0.0;

        if(test_time_us > 0)
        {
            bytes_per_us = static_cast<float>(m_bytes_cnt) / static_cast<float>(test_time_us);
            bytes_per_sec = bytes_per_us * num_1M;
            mb_per_sec = bytes_per_sec / (1024 * 1024);
        }

        return mb_per_sec;
    }

private:
    std::unique_ptr<bsp_perf::common::PerfProfiler> m_profiler{nullptr};
    std::unique_ptr<uint32_t[]> m_test_arr{nullptr};
    size_t m_bytes_cnt{0};
    size_t m_rw_cnt{0};

    float m_smart_write_bw_mb{0.0};
    float m_raw_write_bw_mb{0.0};
    float m_smart_read_bw_mb{0.0};
    float m_raw_read_bw_mb{0.0};
    float m_memcpy_bw_mb{0.0};
};

} // namespace perf_cases
} // namespace bsp_perf

#endif // DDR_PERF_HPP

