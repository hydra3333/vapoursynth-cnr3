#pragma once

#include <cstdint>

#include "cnr3_build_config.h"

// -----------------------------------------------------------------------------
// CNR3 memory diagnostics
//
// These helpers collect process-level and system-level memory diagnostics.
//
// They are intended for development/runtime-behaviour proving, especially while
// evaluating interaction between VapourSynth's own caching and CNR3's v005
// output/checkpoint cache manager.
//
// The measurements are deliberately process/system level. They do not attempt
// to split memory ownership between VapourSynth and CNR3.
//
// The intended use is trend correlation:
//     - compare process memory with CNR3 cache-manager counts;
//     - compare available/used physical memory with Windows Task Manager;
//     - identify unexpected growth before or during v005 cache-manager runtime
//       testing.
//
// These diagnostics are controlled by CNR3_MEMORY_DIAGNOSTICS and are not
// required for correctness.
// -----------------------------------------------------------------------------

struct Cnr3MemorySnapshot {
    bool process_ok = false;
    bool global_ok = false;
    bool performance_ok = false;

    // Process memory, from GetProcessMemoryInfo().
    uint64_t process_working_set_bytes = 0;
    uint64_t process_peak_working_set_bytes = 0;
    uint64_t process_private_usage_bytes = 0;
    uint64_t process_pagefile_usage_bytes = 0;
    uint64_t process_peak_pagefile_usage_bytes = 0;

    // Global/system memory, from GlobalMemoryStatusEx().
    uint32_t system_memory_load_percent = 0;

    uint64_t system_total_phys_bytes = 0;
    uint64_t system_avail_phys_bytes = 0;
    uint64_t system_used_phys_bytes = 0;

    uint64_t system_total_pagefile_bytes = 0;
    uint64_t system_avail_pagefile_bytes = 0;
    uint64_t system_used_pagefile_bytes = 0;

    uint64_t system_total_virtual_bytes = 0;
    uint64_t system_avail_virtual_bytes = 0;
    uint64_t system_used_virtual_bytes = 0;

    // Performance information, from GetPerformanceInfo().
    uint64_t performance_commit_total_bytes = 0;
    uint64_t performance_commit_limit_bytes = 0;
    uint64_t performance_commit_peak_bytes = 0;

    uint64_t performance_physical_total_bytes = 0;
    uint64_t performance_physical_available_bytes = 0;
    uint64_t performance_physical_used_bytes = 0;

    uint64_t performance_system_cache_bytes = 0;
    uint64_t performance_kernel_total_bytes = 0;
    uint64_t performance_kernel_paged_bytes = 0;
    uint64_t performance_kernel_nonpaged_bytes = 0;
};

struct Cnr3MemoryStats {
    uint64_t sample_count = 0;

    bool have_process_working_set = false;
    uint64_t process_working_set_sample_count = 0;
    uint64_t process_working_set_min_bytes = 0;
    uint64_t process_working_set_max_bytes = 0;
    long double process_working_set_sum_bytes = 0.0L;

    bool have_process_private_usage = false;
    uint64_t process_private_usage_sample_count = 0;
    uint64_t process_private_usage_min_bytes = 0;
    uint64_t process_private_usage_max_bytes = 0;
    long double process_private_usage_sum_bytes = 0.0L;

    bool have_system_avail_phys = false;
    uint64_t system_avail_phys_sample_count = 0;
    uint64_t system_avail_phys_min_bytes = 0;
    uint64_t system_avail_phys_max_bytes = 0;
    long double system_avail_phys_sum_bytes = 0.0L;

    bool have_system_used_phys = false;
    uint64_t system_used_phys_sample_count = 0;
    uint64_t system_used_phys_min_bytes = 0;
    uint64_t system_used_phys_max_bytes = 0;
    long double system_used_phys_sum_bytes = 0.0L;

    bool have_commit_total = false;
    uint64_t commit_total_sample_count = 0;
    uint64_t commit_total_min_bytes = 0;
    uint64_t commit_total_max_bytes = 0;
    long double commit_total_sum_bytes = 0.0L;
};

bool cnr3_memory_take_snapshot(
    Cnr3MemorySnapshot& snapshot
);

void cnr3_memory_accumulate_snapshot(
    Cnr3MemoryStats& stats,
    const Cnr3MemorySnapshot& snapshot
);

void cnr3_memory_record_and_print_snapshot(
    Cnr3MemoryStats& stats,
    bool debug_enabled,
    int instance_id,
    const char* where
);

void cnr3_memory_print_summary(
    const Cnr3MemoryStats& stats,
    bool debug_enabled,
    int instance_id,
    const char* where
);
