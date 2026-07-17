#include "cnr3_memory_diagnostics.h"

#if defined(CNR3_DIAG_COMPUTE_DSUM02_MEMORY)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "cnr3_diagnostics.h"

#include <array>
#include <cstdio>
#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "Psapi.lib")

namespace {

inline constexpr const char* CNR3_DSUM02_COMPONENT = "D-SUM-02";
inline constexpr const char* CNR3_DSUM02_SNAPSHOT_TAG = "[DSUM02-SNAPSHOT]";
inline constexpr const char* CNR3_DSUM02_SUMMARY_TAG = "[DSUM02-SUMMARY]";

inline constexpr int kMemMetricW = 24;
inline constexpr int kMemValW = 14;
inline constexpr int kMemPctW = 11;

[[nodiscard]] double cnr3_memory_bytes_to_mb(
    std::uint64_t bytes
) noexcept {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

[[nodiscard]] double cnr3_memory_metric_average(
    const Cnr3MemoryMetricStats& metric
) noexcept {
    if (metric.sample_count == 0U) {
        return 0.0;
    }

    return static_cast<double>(
        metric.sum_value / static_cast<long double>(metric.sample_count)
    );
}

[[nodiscard]] std::uint64_t cnr3_memory_pages_to_bytes(
    SIZE_T pages,
    SIZE_T page_size
) noexcept {
    return
        static_cast<std::uint64_t>(pages) *
        static_cast<std::uint64_t>(page_size);
}

void cnr3_memory_accumulate_metric(
    Cnr3MemoryMetricStats& metric,
    long double value
) noexcept {
    ++metric.sample_count;

    if (!metric.have_value) {
        metric.have_value = true;
        metric.min_value = value;
        metric.max_value = value;
        metric.sum_value = value;
        return;
    }

    if (value < metric.min_value) {
        metric.min_value = value;
    }

    if (value > metric.max_value) {
        metric.max_value = value;
    }

    metric.sum_value += value;
}

void cnr3_memory_accumulate_peak(
    bool& have_value,
    std::uint64_t& max_bytes,
    std::uint64_t value_bytes
) noexcept {
    if (!have_value) {
        have_value = true;
        max_bytes = value_bytes;
        return;
    }

    if (value_bytes > max_bytes) {
        max_bytes = value_bytes;
    }
}

[[nodiscard]] double cnr3_memory_delta_percent(
    double current_value,
    double baseline_value
) noexcept {
    if (baseline_value <= 0.0) {
        return 0.0;
    }

    return ((current_value - baseline_value) / baseline_value) * 100.0;
}

[[nodiscard]] double cnr3_memory_min_to_max_percent(
    double min_value,
    double max_value
) noexcept {
    if (min_value <= 0.0) {
        return 0.0;
    }

    return ((max_value - min_value) / min_value) * 100.0;
}

void cnr3_memory_write_line(
    Cnr3InstanceId instance_id,
    const char* tag,
    const char* text
) noexcept {
#if defined(CNR3_DIAG_PRINT_DSUM02_MEMORY)
    std::array<char, 768> line{};

    std::snprintf(
        line.data(),
        line.size(),
        "%s %s",
        tag != nullptr ? tag : CNR3_DSUM02_SNAPSHOT_TAG,
        text != nullptr ? text : ""
    );

    cnr3_diag_write_line(
        instance_id,
        Cnr3DiagnosticLevel::info,
        CNR3_DSUM02_COMPONENT,
        line.data(),
        Cnr3StderrFlushPolicy::no_flush
    );
#else
    (void)instance_id;
    (void)tag;
    (void)text;
#endif
}

void cnr3_memory_write_snapshot_text(
    Cnr3InstanceId instance_id,
    const char* text
) noexcept {
    cnr3_memory_write_line(instance_id, CNR3_DSUM02_SNAPSHOT_TAG, text);
}

void cnr3_memory_write_summary_text(
    Cnr3InstanceId instance_id,
    const char* text
) noexcept {
    cnr3_memory_write_line(instance_id, CNR3_DSUM02_SUMMARY_TAG, text);
}

void cnr3_memory_write_snapshot_header(
    Cnr3InstanceId instance_id,
    const char* label
) noexcept {
    std::array<char, 256> line{};

    std::snprintf(
        line.data(),
        line.size(),
        "CNR3 memory: instance=%d, %s",
        cnr3_instance_id_is_valid(instance_id) ? instance_id.value : 0,
        label != nullptr ? label : "snapshot"
    );

    cnr3_memory_write_snapshot_text(instance_id, line.data());

    std::snprintf(
        line.data(),
        line.size(),
        "  %-*s %*s %*s %*s %*s",
        kMemMetricW,
        "Metric",
        kMemValW,
        "Now (MB)",
        kMemValW,
        "Start (MB)",
        kMemValW,
        "Delta(MB)",
        kMemPctW,
        "Delta(%)"
    );

    cnr3_memory_write_snapshot_text(instance_id, line.data());
}

void cnr3_memory_write_snapshot_row(
    Cnr3InstanceId instance_id,
    const char* metric_name,
    double current_value,
    double baseline_value,
    const char* suffix = nullptr
) noexcept {
    const double delta_value = current_value - baseline_value;
    const double delta_percent = cnr3_memory_delta_percent(current_value, baseline_value);
    std::array<char, 256> line{};

    if (suffix != nullptr) {
        std::snprintf(
            line.data(),
            line.size(),
            "  %-*s %*.2f %*.2f %+*.2f %+*.2f   %s",
            kMemMetricW,
            metric_name != nullptr ? metric_name : "<unknown>",
            kMemValW,
            current_value,
            kMemValW,
            baseline_value,
            kMemValW,
            delta_value,
            kMemPctW,
            delta_percent,
            suffix
        );
    }
    else {
        std::snprintf(
            line.data(),
            line.size(),
            "  %-*s %*.2f %*.2f %+*.2f %+*.2f",
            kMemMetricW,
            metric_name != nullptr ? metric_name : "<unknown>",
            kMemValW,
            current_value,
            kMemValW,
            baseline_value,
            kMemValW,
            delta_value,
            kMemPctW,
            delta_percent
        );
    }

    cnr3_memory_write_snapshot_text(instance_id, line.data());
}

void cnr3_memory_write_snapshot_bytes_row(
    Cnr3InstanceId instance_id,
    const char* metric_name,
    std::uint64_t current_bytes,
    std::uint64_t baseline_bytes
) noexcept {
    cnr3_memory_write_snapshot_row(
        instance_id,
        metric_name,
        cnr3_memory_bytes_to_mb(current_bytes),
        cnr3_memory_bytes_to_mb(baseline_bytes)
    );
}

void cnr3_memory_write_snapshot_peak_row(
    Cnr3InstanceId instance_id,
    const char* metric_name,
    std::uint64_t current_bytes
) noexcept {
    std::array<char, 256> line{};

    std::snprintf(
        line.data(),
        line.size(),
        "  %-*s %*.2f   (cumulative peak, no delta)",
        kMemMetricW,
        metric_name != nullptr ? metric_name : "<unknown>",
        kMemValW,
        cnr3_memory_bytes_to_mb(current_bytes)
    );

    cnr3_memory_write_snapshot_text(instance_id, line.data());
}

void cnr3_memory_write_snapshot_value_note_row(
    Cnr3InstanceId instance_id,
    const char* metric_name,
    std::uint64_t current_bytes,
    const char* note
) noexcept {
    std::array<char, 256> line{};

    std::snprintf(
        line.data(),
        line.size(),
        "  %-*s %*.2f   %s",
        kMemMetricW,
        metric_name != nullptr ? metric_name : "<unknown>",
        kMemValW,
        cnr3_memory_bytes_to_mb(current_bytes),
        note != nullptr ? note : ""
    );

    cnr3_memory_write_snapshot_text(instance_id, line.data());
}

void cnr3_memory_write_static_totals_snapshot(
    Cnr3InstanceId instance_id,
    const Cnr3MemorySnapshot& snapshot
) noexcept {
    std::array<char, 256> line{};

    cnr3_memory_write_snapshot_text(instance_id, "  Static totals (MB):");

    const auto write_static = [&](const char* metric_name, std::uint64_t value_bytes) noexcept {
        std::snprintf(
            line.data(),
            line.size(),
            "  %-*s %*.2f",
            kMemMetricW,
            metric_name,
            kMemValW,
            cnr3_memory_bytes_to_mb(value_bytes)
        );
        cnr3_memory_write_snapshot_text(instance_id, line.data());
    };

    write_static("system_total_phys", snapshot.system_total_phys_bytes);
    write_static("perf_kernel_total", snapshot.performance_kernel_total_bytes);
}

void cnr3_memory_write_legend_snapshot(
    Cnr3InstanceId instance_id
) noexcept {
    cnr3_memory_write_snapshot_text(instance_id, "  Legend:");
    cnr3_memory_write_snapshot_text(instance_id, "  process_working_set    RAM actively mapped to this process; drops after cache release; persistent delta above baseline suggests leak.");
    cnr3_memory_write_snapshot_text(instance_id, "  process_private_usage  Best process-level memory-growth indicator; should broadly correlate with cache growth but is not cache-only.");
    cnr3_memory_write_snapshot_text(instance_id, "  system_avail_phys      Free physical RAM system-wide; falls as the process/system uses more; small percent change is normal.");
    cnr3_memory_write_snapshot_text(instance_id, "  system_used_phys       Physical RAM in use system-wide; mirror of avail_phys; helps confirm system-level impact.");
    cnr3_memory_write_snapshot_text(instance_id, "  commit_total           Total committed virtual memory system-wide; can grow with cache and should mostly recover after cleanup.");
    cnr3_memory_write_snapshot_text(instance_id, "  peak_working_set       Highest working_set seen this run; reveals worst-case RAM pressure from processing.");
    cnr3_memory_write_snapshot_text(instance_id, "  peak_private_usage     Highest private committed memory seen this run; compare with after-cleanup value.");
    cnr3_memory_write_snapshot_text(instance_id, "  Min->Max (%)           Percentage spread from minimum to maximum sample; shows movement during the run, not proof of a leak.");
    cnr3_memory_write_snapshot_text(instance_id, "  other_statistics       Secondary dynamic metrics accumulated for future analysis; interpretive only.");
    cnr3_memory_write_snapshot_text(instance_id, "  post-cleanup caveat    CNR3 releases its frame references; VapourSynth may still hold buffers in its own cache/pool.");
}

void cnr3_memory_write_legend_summary(
    Cnr3InstanceId instance_id
) noexcept {
    cnr3_memory_write_summary_text(instance_id, "  Legend:");
    cnr3_memory_write_summary_text(instance_id, "  process_working_set    RAM actively mapped to this process; drops after cache release; persistent delta above baseline suggests leak.");
    cnr3_memory_write_summary_text(instance_id, "  process_private_usage  Best process-level memory-growth indicator; should broadly correlate with cache growth but is not cache-only.");
    cnr3_memory_write_summary_text(instance_id, "  system_avail_phys      Free physical RAM system-wide; falls as the process/system uses more; small percent change is normal.");
    cnr3_memory_write_summary_text(instance_id, "  system_used_phys       Physical RAM in use system-wide; mirror of avail_phys; helps confirm system-level impact.");
    cnr3_memory_write_summary_text(instance_id, "  commit_total           Total committed virtual memory system-wide; can grow with cache and should mostly recover after cleanup.");
    cnr3_memory_write_summary_text(instance_id, "  peak_working_set       Highest working_set seen this run; reveals worst-case RAM pressure from processing.");
    cnr3_memory_write_summary_text(instance_id, "  peak_private_usage     Highest private committed memory seen this run; compare with after-cleanup value.");
    cnr3_memory_write_summary_text(instance_id, "  Min->Max (%)           Percentage spread from minimum to maximum sample; shows movement during the run, not proof of a leak.");
    cnr3_memory_write_summary_text(instance_id, "  other_statistics       Secondary dynamic metrics accumulated for future analysis; interpretive only.");
    cnr3_memory_write_summary_text(instance_id, "  post-cleanup caveat    CNR3 releases its frame references; VapourSynth may still hold buffers in its own cache/pool.");
}

void cnr3_memory_write_snapshot_table(
    Cnr3InstanceId instance_id,
    const Cnr3MemoryStats& stats,
    const Cnr3MemorySnapshot& snapshot,
    const char* label,
    bool show_legend
) noexcept {
    const Cnr3MemorySnapshot& baseline = stats.baseline;

    cnr3_memory_write_snapshot_header(instance_id, label);

    cnr3_memory_write_snapshot_value_note_row(instance_id, "Total Physical Memory", snapshot.system_total_phys_bytes, "(machine RAM, static)");
    cnr3_memory_write_snapshot_bytes_row(instance_id, "system_avail_phys", snapshot.system_avail_phys_bytes, baseline.system_avail_phys_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "system_used_phys", snapshot.system_used_phys_bytes, baseline.system_used_phys_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "process_working_set", snapshot.process_working_set_bytes, baseline.process_working_set_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "process_private_usage", snapshot.process_private_usage_bytes, baseline.process_private_usage_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "commit_total", snapshot.performance_commit_total_bytes, baseline.performance_commit_total_bytes);
    cnr3_memory_write_snapshot_peak_row(instance_id, "peak_working_set", snapshot.process_peak_working_set_bytes);
    cnr3_memory_write_snapshot_peak_row(instance_id, "peak_private_usage", snapshot.process_peak_pagefile_usage_bytes);

    cnr3_memory_write_snapshot_text(instance_id, "  Other memory statistics:");
    cnr3_memory_write_snapshot_row(instance_id, "system_memory_load_pct", static_cast<double>(snapshot.system_memory_load_percent), static_cast<double>(baseline.system_memory_load_percent), "(percent, not MB)");
    cnr3_memory_write_snapshot_bytes_row(instance_id, "system_avail_virtual", snapshot.system_avail_virtual_bytes, baseline.system_avail_virtual_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "system_used_virtual", snapshot.system_used_virtual_bytes, baseline.system_used_virtual_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "perf_physical_avail", snapshot.performance_physical_available_bytes, baseline.performance_physical_available_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "perf_physical_used", snapshot.performance_physical_used_bytes, baseline.performance_physical_used_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "perf_system_cache", snapshot.performance_system_cache_bytes, baseline.performance_system_cache_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "perf_kernel_paged", snapshot.performance_kernel_paged_bytes, baseline.performance_kernel_paged_bytes);
    cnr3_memory_write_snapshot_bytes_row(instance_id, "perf_kernel_nonpaged", snapshot.performance_kernel_nonpaged_bytes, baseline.performance_kernel_nonpaged_bytes);
    cnr3_memory_write_snapshot_peak_row(instance_id, "commit_peak", snapshot.performance_commit_peak_bytes);

    if (show_legend) {
        cnr3_memory_write_static_totals_snapshot(instance_id, baseline);
        cnr3_memory_write_legend_snapshot(instance_id);
    }
}

void cnr3_memory_write_summary_header(
    Cnr3InstanceId instance_id,
    const Cnr3MemoryStats& stats
) noexcept {
    std::array<char, 256> line{};

    std::snprintf(
        line.data(),
        line.size(),
        "CNR3 memory: instance=%d, summary (%llu samples)",
        cnr3_instance_id_is_valid(instance_id) ? instance_id.value : 0,
        static_cast<unsigned long long>(stats.sample_count)
    );

    cnr3_memory_write_summary_text(instance_id, line.data());

    std::snprintf(
        line.data(),
        line.size(),
        "  %-*s %*s %*s %*s %*s",
        kMemMetricW,
        "Metric",
        kMemValW,
        "Min",
        kMemValW,
        "Avg",
        kMemValW,
        "Max",
        kMemPctW,
        "Min->Max (%)"
    );

    cnr3_memory_write_summary_text(instance_id, line.data());
}

void cnr3_memory_write_summary_metric_row(
    Cnr3InstanceId instance_id,
    const char* metric_name,
    const Cnr3MemoryMetricStats& metric,
    bool bytes_to_mb,
    const char* suffix = nullptr
) noexcept {
    const double min_value = metric.have_value
        ? (bytes_to_mb ? cnr3_memory_bytes_to_mb(static_cast<std::uint64_t>(metric.min_value)) : static_cast<double>(metric.min_value))
        : 0.0;
    const double avg_value = metric.have_value
        ? (bytes_to_mb ? cnr3_memory_bytes_to_mb(static_cast<std::uint64_t>(cnr3_memory_metric_average(metric))) : cnr3_memory_metric_average(metric))
        : 0.0;
    const double max_value = metric.have_value
        ? (bytes_to_mb ? cnr3_memory_bytes_to_mb(static_cast<std::uint64_t>(metric.max_value)) : static_cast<double>(metric.max_value))
        : 0.0;
    const double min_to_max_percent = metric.have_value
        ? cnr3_memory_min_to_max_percent(min_value, max_value)
        : 0.0;
    std::array<char, 256> line{};

    if (suffix != nullptr) {
        std::snprintf(
            line.data(),
            line.size(),
            "  %-*s %*.2f %*.2f %*.2f %+*.2f   %s",
            kMemMetricW,
            metric_name != nullptr ? metric_name : "<unknown>",
            kMemValW,
            min_value,
            kMemValW,
            avg_value,
            kMemValW,
            max_value,
            kMemPctW,
            min_to_max_percent,
            suffix
        );
    }
    else {
        std::snprintf(
            line.data(),
            line.size(),
            "  %-*s %*.2f %*.2f %*.2f %+*.2f",
            kMemMetricW,
            metric_name != nullptr ? metric_name : "<unknown>",
            kMemValW,
            min_value,
            kMemValW,
            avg_value,
            kMemValW,
            max_value,
            kMemPctW,
            min_to_max_percent
        );
    }

    cnr3_memory_write_summary_text(instance_id, line.data());
}

void cnr3_memory_write_summary_peak_row(
    Cnr3InstanceId instance_id,
    const char* metric_name,
    bool have_value,
    std::uint64_t max_bytes
) noexcept {
    std::array<char, 256> line{};

    std::snprintf(
        line.data(),
        line.size(),
        "  %-*s %*.2f   (running max, no min/avg)",
        kMemMetricW,
        metric_name != nullptr ? metric_name : "<unknown>",
        kMemValW,
        have_value ? cnr3_memory_bytes_to_mb(max_bytes) : 0.0
    );

    cnr3_memory_write_summary_text(instance_id, line.data());
}

void cnr3_memory_write_summary_value_note_row(
    Cnr3InstanceId instance_id,
    const char* metric_name,
    std::uint64_t current_bytes,
    const char* note
) noexcept {
    std::array<char, 256> line{};

    std::snprintf(
        line.data(),
        line.size(),
        "  %-*s %*.2f   %s",
        kMemMetricW,
        metric_name != nullptr ? metric_name : "<unknown>",
        kMemValW,
        cnr3_memory_bytes_to_mb(current_bytes),
        note != nullptr ? note : ""
    );

    cnr3_memory_write_summary_text(instance_id, line.data());
}

void cnr3_memory_write_static_totals_summary(
    Cnr3InstanceId instance_id,
    const Cnr3MemorySnapshot& baseline
) noexcept {
    std::array<char, 256> line{};

    cnr3_memory_write_summary_text(instance_id, "  Static totals (MB):");

    const auto write_static = [&](const char* metric_name, std::uint64_t value_bytes) noexcept {
        std::snprintf(
            line.data(),
            line.size(),
            "  %-*s %*.2f",
            kMemMetricW,
            metric_name,
            kMemValW,
            cnr3_memory_bytes_to_mb(value_bytes)
        );
        cnr3_memory_write_summary_text(instance_id, line.data());
    };

    write_static("system_total_phys", baseline.system_total_phys_bytes);
    write_static("perf_kernel_total", baseline.performance_kernel_total_bytes);
}

} // namespace

bool cnr3_memory_take_snapshot(
    Cnr3MemorySnapshot& snapshot
) noexcept {
    snapshot = Cnr3MemorySnapshot{};

    PROCESS_MEMORY_COUNTERS_EX process_memory{};
    process_memory.cb = sizeof(process_memory);

    if (GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process_memory),
        sizeof(process_memory))) {
        snapshot.process_ok = true;
        snapshot.process_working_set_bytes = static_cast<std::uint64_t>(process_memory.WorkingSetSize);
        snapshot.process_peak_working_set_bytes = static_cast<std::uint64_t>(process_memory.PeakWorkingSetSize);
        snapshot.process_private_usage_bytes = static_cast<std::uint64_t>(process_memory.PrivateUsage);
        snapshot.process_pagefile_usage_bytes = static_cast<std::uint64_t>(process_memory.PagefileUsage);
        snapshot.process_peak_pagefile_usage_bytes = static_cast<std::uint64_t>(process_memory.PeakPagefileUsage);
    }

    MEMORYSTATUSEX global_memory{};
    global_memory.dwLength = sizeof(global_memory);

    if (GlobalMemoryStatusEx(&global_memory)) {
        snapshot.global_ok = true;
        snapshot.system_memory_load_percent = static_cast<std::uint32_t>(global_memory.dwMemoryLoad);
        snapshot.system_total_phys_bytes = static_cast<std::uint64_t>(global_memory.ullTotalPhys);
        snapshot.system_avail_phys_bytes = static_cast<std::uint64_t>(global_memory.ullAvailPhys);
        snapshot.system_used_phys_bytes = snapshot.system_total_phys_bytes - snapshot.system_avail_phys_bytes;
        snapshot.system_total_pagefile_bytes = static_cast<std::uint64_t>(global_memory.ullTotalPageFile);
        snapshot.system_avail_pagefile_bytes = static_cast<std::uint64_t>(global_memory.ullAvailPageFile);
        snapshot.system_used_pagefile_bytes = snapshot.system_total_pagefile_bytes - snapshot.system_avail_pagefile_bytes;
        snapshot.system_total_virtual_bytes = static_cast<std::uint64_t>(global_memory.ullTotalVirtual);
        snapshot.system_avail_virtual_bytes = static_cast<std::uint64_t>(global_memory.ullAvailVirtual);
        snapshot.system_used_virtual_bytes = snapshot.system_total_virtual_bytes - snapshot.system_avail_virtual_bytes;
    }

    PERFORMANCE_INFORMATION performance_info{};
    performance_info.cb = sizeof(performance_info);

    if (GetPerformanceInfo(&performance_info, sizeof(performance_info))) {
        snapshot.performance_ok = true;
        const SIZE_T page_size = performance_info.PageSize;

        snapshot.performance_commit_total_bytes = cnr3_memory_pages_to_bytes(performance_info.CommitTotal, page_size);
        snapshot.performance_commit_limit_bytes = cnr3_memory_pages_to_bytes(performance_info.CommitLimit, page_size);
        snapshot.performance_commit_peak_bytes = cnr3_memory_pages_to_bytes(performance_info.CommitPeak, page_size);
        snapshot.performance_physical_total_bytes = cnr3_memory_pages_to_bytes(performance_info.PhysicalTotal, page_size);
        snapshot.performance_physical_available_bytes = cnr3_memory_pages_to_bytes(performance_info.PhysicalAvailable, page_size);
        snapshot.performance_physical_used_bytes = snapshot.performance_physical_total_bytes - snapshot.performance_physical_available_bytes;
        snapshot.performance_system_cache_bytes = cnr3_memory_pages_to_bytes(performance_info.SystemCache, page_size);
        snapshot.performance_kernel_total_bytes = cnr3_memory_pages_to_bytes(performance_info.KernelTotal, page_size);
        snapshot.performance_kernel_paged_bytes = cnr3_memory_pages_to_bytes(performance_info.KernelPaged, page_size);
        snapshot.performance_kernel_nonpaged_bytes = cnr3_memory_pages_to_bytes(performance_info.KernelNonpaged, page_size);
    }

    return snapshot.process_ok || snapshot.global_ok || snapshot.performance_ok;
}

void cnr3_memory_accumulate_snapshot(
    Cnr3MemoryStats& stats,
    const Cnr3MemorySnapshot& snapshot
) noexcept {
    ++stats.sample_count;

    if (!stats.baseline_valid) {
        stats.baseline_valid = true;
        stats.baseline = snapshot;
    }

    if (snapshot.process_ok) {
        cnr3_memory_accumulate_metric(stats.process_working_set, static_cast<long double>(snapshot.process_working_set_bytes));
        cnr3_memory_accumulate_metric(stats.process_private_usage, static_cast<long double>(snapshot.process_private_usage_bytes));
        cnr3_memory_accumulate_peak(stats.have_peak_working_set, stats.peak_working_set_max_bytes, snapshot.process_peak_working_set_bytes);
        cnr3_memory_accumulate_peak(stats.have_peak_private_usage, stats.peak_private_usage_max_bytes, snapshot.process_peak_pagefile_usage_bytes);
    }

    if (snapshot.global_ok) {
        cnr3_memory_accumulate_metric(stats.system_memory_load_pct, static_cast<long double>(snapshot.system_memory_load_percent));
        cnr3_memory_accumulate_metric(stats.system_avail_phys, static_cast<long double>(snapshot.system_avail_phys_bytes));
        cnr3_memory_accumulate_metric(stats.system_used_phys, static_cast<long double>(snapshot.system_used_phys_bytes));
        cnr3_memory_accumulate_metric(stats.system_avail_virtual, static_cast<long double>(snapshot.system_avail_virtual_bytes));
        cnr3_memory_accumulate_metric(stats.system_used_virtual, static_cast<long double>(snapshot.system_used_virtual_bytes));
    }

    if (snapshot.performance_ok) {
        cnr3_memory_accumulate_metric(stats.commit_total, static_cast<long double>(snapshot.performance_commit_total_bytes));
        cnr3_memory_accumulate_metric(stats.perf_physical_avail, static_cast<long double>(snapshot.performance_physical_available_bytes));
        cnr3_memory_accumulate_metric(stats.perf_physical_used, static_cast<long double>(snapshot.performance_physical_used_bytes));
        cnr3_memory_accumulate_metric(stats.perf_system_cache, static_cast<long double>(snapshot.performance_system_cache_bytes));
        cnr3_memory_accumulate_metric(stats.perf_kernel_paged, static_cast<long double>(snapshot.performance_kernel_paged_bytes));
        cnr3_memory_accumulate_metric(stats.perf_kernel_nonpaged, static_cast<long double>(snapshot.performance_kernel_nonpaged_bytes));
        cnr3_memory_accumulate_peak(stats.have_commit_peak, stats.commit_peak_max_bytes, snapshot.performance_commit_peak_bytes);
    }
}

void cnr3_memory_record_and_print_snapshot(
    Cnr3MemoryStats& stats,
    Cnr3InstanceId instance_id,
    const char* label,
    bool show_legend
) noexcept {
    Cnr3MemorySnapshot snapshot{};

    if (!cnr3_memory_take_snapshot(snapshot)) {
#if defined(CNR3_DIAG_PRINT_DSUM02_MEMORY)
        cnr3_memory_write_snapshot_text(instance_id, "snapshot unavailable");
        cnr3_diag_flush_stderr();
#endif
        return;
    }

    cnr3_memory_accumulate_snapshot(stats, snapshot);

#if defined(CNR3_DIAG_PRINT_DSUM02_MEMORY)
    cnr3_memory_write_snapshot_table(instance_id, stats, snapshot, label, show_legend);
    cnr3_diag_flush_stderr();
#else
    (void)instance_id;
    (void)label;
    (void)show_legend;
#endif
}

void cnr3_memory_print_summary(
    const Cnr3MemoryStats& stats,
    Cnr3InstanceId instance_id
) noexcept {
#if defined(CNR3_DIAG_PRINT_DSUM02_MEMORY)
    if (stats.sample_count == 0U || !stats.baseline_valid) {
        cnr3_memory_write_summary_text(instance_id, "CNR3 memory: summary (0 samples)");
        cnr3_diag_flush_stderr();
        return;
    }

    cnr3_memory_write_summary_header(instance_id, stats);
    cnr3_memory_write_summary_value_note_row(instance_id, "Total Physical Memory", stats.baseline.system_total_phys_bytes, "(machine RAM, static)");
    cnr3_memory_write_summary_metric_row(instance_id, "system_avail_phys", stats.system_avail_phys, true);
    cnr3_memory_write_summary_metric_row(instance_id, "system_used_phys", stats.system_used_phys, true);
    cnr3_memory_write_summary_metric_row(instance_id, "process_working_set", stats.process_working_set, true);
    cnr3_memory_write_summary_metric_row(instance_id, "process_private_usage", stats.process_private_usage, true);
    cnr3_memory_write_summary_metric_row(instance_id, "commit_total", stats.commit_total, true);
    cnr3_memory_write_summary_peak_row(instance_id, "peak_working_set", stats.have_peak_working_set, stats.peak_working_set_max_bytes);
    cnr3_memory_write_summary_peak_row(instance_id, "peak_private_usage", stats.have_peak_private_usage, stats.peak_private_usage_max_bytes);

    cnr3_memory_write_summary_text(instance_id, "  Other memory statistics:");
    cnr3_memory_write_summary_metric_row(instance_id, "system_memory_load_pct", stats.system_memory_load_pct, false, "(percent, not MB)");
    cnr3_memory_write_summary_metric_row(instance_id, "system_avail_virtual", stats.system_avail_virtual, true);
    cnr3_memory_write_summary_metric_row(instance_id, "system_used_virtual", stats.system_used_virtual, true);
    cnr3_memory_write_summary_metric_row(instance_id, "perf_physical_avail", stats.perf_physical_avail, true);
    cnr3_memory_write_summary_metric_row(instance_id, "perf_physical_used", stats.perf_physical_used, true);
    cnr3_memory_write_summary_metric_row(instance_id, "perf_system_cache", stats.perf_system_cache, true);
    cnr3_memory_write_summary_metric_row(instance_id, "perf_kernel_paged", stats.perf_kernel_paged, true);
    cnr3_memory_write_summary_metric_row(instance_id, "perf_kernel_nonpaged", stats.perf_kernel_nonpaged, true);
    cnr3_memory_write_summary_peak_row(instance_id, "commit_peak", stats.have_commit_peak, stats.commit_peak_max_bytes);

    cnr3_memory_write_static_totals_summary(instance_id, stats.baseline);
    cnr3_memory_write_legend_summary(instance_id);
    cnr3_diag_flush_stderr();
#else
    (void)stats;
    (void)instance_id;
#endif
}

#endif
