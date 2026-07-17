#pragma once

#include "cnr3_build_config.h"
#include "cnr3_common.h"

#include <cstdint>

/*
    CNR3 D-SUM-02 memory diagnostics.

    This module samples process/system memory at filter lifecycle points and
    emits formatted D-SUM-02 tables through the generic diagnostics boundary.

    The diagnostics are compile-gated only. They observe process/system memory
    movement and do not attempt to attribute memory ownership between
    VapourSynth and CNR3.
*/

#if defined(CNR3_DIAG_COMPUTE_DSUM02_MEMORY)

struct Cnr3MemorySnapshot {
    bool process_ok = false;
    bool global_ok = false;
    bool performance_ok = false;

    // Process memory, from GetProcessMemoryInfo().
    std::uint64_t process_working_set_bytes = 0;
    std::uint64_t process_peak_working_set_bytes = 0;
    std::uint64_t process_private_usage_bytes = 0;
    std::uint64_t process_pagefile_usage_bytes = 0;
    std::uint64_t process_peak_pagefile_usage_bytes = 0;

    // Global/system memory, from GlobalMemoryStatusEx().
    std::uint32_t system_memory_load_percent = 0;

    std::uint64_t system_total_phys_bytes = 0;
    std::uint64_t system_avail_phys_bytes = 0;
    std::uint64_t system_used_phys_bytes = 0;

    std::uint64_t system_total_pagefile_bytes = 0;
    std::uint64_t system_avail_pagefile_bytes = 0;
    std::uint64_t system_used_pagefile_bytes = 0;

    std::uint64_t system_total_virtual_bytes = 0;
    std::uint64_t system_avail_virtual_bytes = 0;
    std::uint64_t system_used_virtual_bytes = 0;

    // Performance information, from GetPerformanceInfo().
    std::uint64_t performance_commit_total_bytes = 0;
    std::uint64_t performance_commit_limit_bytes = 0;
    std::uint64_t performance_commit_peak_bytes = 0;

    std::uint64_t performance_physical_total_bytes = 0;
    std::uint64_t performance_physical_available_bytes = 0;
    std::uint64_t performance_physical_used_bytes = 0;

    std::uint64_t performance_system_cache_bytes = 0;
    std::uint64_t performance_kernel_total_bytes = 0;
    std::uint64_t performance_kernel_paged_bytes = 0;
    std::uint64_t performance_kernel_nonpaged_bytes = 0;
};

struct Cnr3MemoryMetricStats {
    bool have_value = false;
    std::uint64_t sample_count = 0;
    long double min_value = 0.0L;
    long double max_value = 0.0L;
    long double sum_value = 0.0L;
};

struct Cnr3MemoryStats {
    std::uint64_t sample_count = 0;

    bool baseline_valid = false;
    Cnr3MemorySnapshot baseline{};

    // Dynamic metrics: accumulated as min/average/max.
    Cnr3MemoryMetricStats process_working_set{};
    Cnr3MemoryMetricStats process_private_usage{};
    Cnr3MemoryMetricStats system_memory_load_pct{};
    Cnr3MemoryMetricStats system_avail_phys{};
    Cnr3MemoryMetricStats system_used_phys{};
    Cnr3MemoryMetricStats system_avail_virtual{};
    Cnr3MemoryMetricStats system_used_virtual{};
    Cnr3MemoryMetricStats commit_total{};
    Cnr3MemoryMetricStats perf_physical_avail{};
    Cnr3MemoryMetricStats perf_physical_used{};
    Cnr3MemoryMetricStats perf_system_cache{};
    Cnr3MemoryMetricStats perf_kernel_paged{};
    Cnr3MemoryMetricStats perf_kernel_nonpaged{};

    // Peak metrics: retained as running maxima.
    bool have_peak_working_set = false;
    bool have_peak_private_usage = false;
    bool have_commit_peak = false;
    std::uint64_t peak_working_set_max_bytes = 0;
    std::uint64_t peak_private_usage_max_bytes = 0;
    std::uint64_t commit_peak_max_bytes = 0;
};

[[nodiscard]] bool cnr3_memory_take_snapshot(
    Cnr3MemorySnapshot& snapshot
) noexcept;

void cnr3_memory_accumulate_snapshot(
    Cnr3MemoryStats& stats,
    const Cnr3MemorySnapshot& snapshot
) noexcept;

void cnr3_memory_record_and_print_snapshot(
    Cnr3MemoryStats& stats,
    Cnr3InstanceId instance_id,
    const char* label,
    bool show_legend
) noexcept;

void cnr3_memory_print_summary(
    const Cnr3MemoryStats& stats,
    Cnr3InstanceId instance_id
) noexcept;

#endif
