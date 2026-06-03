#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "cnr3_build_config.h"
#include "cnr3_output_cache_manager.h"
#include "cnr3_memory_diagnostics.h"

#include <cstdio>

#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "Psapi.lib")

static double cnr3_memory_bytes_to_mb(
    uint64_t bytes
) {
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

static double cnr3_memory_average_bytes_to_mb(
    long double sum_bytes,
    uint64_t sample_count
) {
    if (sample_count == 0) {
        return 0.0;
    }

    return cnr3_memory_bytes_to_mb(
        static_cast<uint64_t>(
            sum_bytes / static_cast<long double>(sample_count)
            )
    );
}

static uint64_t cnr3_memory_pages_to_bytes(
    SIZE_T pages,
    SIZE_T page_size
) {
    return
        static_cast<uint64_t>(pages) *
        static_cast<uint64_t>(page_size);
}

static void cnr3_memory_update_min_max_sum(
    bool& have_value,
    uint64_t& sample_count,
    uint64_t& min_bytes,
    uint64_t& max_bytes,
    long double& sum_bytes,
    uint64_t value_bytes
) {
    ++sample_count;

    if (!have_value) {
        have_value = true;
        min_bytes = value_bytes;
        max_bytes = value_bytes;
        sum_bytes = static_cast<long double>(value_bytes);
        return;
    }

    if (value_bytes < min_bytes) {
        min_bytes = value_bytes;
    }

    if (value_bytes > max_bytes) {
        max_bytes = value_bytes;
    }

    sum_bytes += static_cast<long double>(value_bytes);
}

bool cnr3_memory_take_snapshot(
    Cnr3MemorySnapshot& snapshot
) {
    if constexpr (!CNR3_MEMORY_DIAGNOSTICS) {
        (void)snapshot;
        return false;
    }
    else {
        snapshot = Cnr3MemorySnapshot{};

        PROCESS_MEMORY_COUNTERS_EX process_memory;
        process_memory.cb = sizeof(process_memory);

        if (
            GetProcessMemoryInfo(
                GetCurrentProcess(),
                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process_memory),
                sizeof(process_memory)
            )
            ) {
            snapshot.process_ok = true;

            snapshot.process_working_set_bytes =
                static_cast<uint64_t>(process_memory.WorkingSetSize);

            snapshot.process_peak_working_set_bytes =
                static_cast<uint64_t>(process_memory.PeakWorkingSetSize);

            snapshot.process_private_usage_bytes =
                static_cast<uint64_t>(process_memory.PrivateUsage);

            snapshot.process_pagefile_usage_bytes =
                static_cast<uint64_t>(process_memory.PagefileUsage);

            snapshot.process_peak_pagefile_usage_bytes =
                static_cast<uint64_t>(process_memory.PeakPagefileUsage);
        }

        MEMORYSTATUSEX global_memory;
        global_memory.dwLength = sizeof(global_memory);

        if (GlobalMemoryStatusEx(&global_memory)) {
            snapshot.global_ok = true;

            snapshot.system_memory_load_percent =
                static_cast<uint32_t>(global_memory.dwMemoryLoad);

            snapshot.system_total_phys_bytes =
                static_cast<uint64_t>(global_memory.ullTotalPhys);

            snapshot.system_avail_phys_bytes =
                static_cast<uint64_t>(global_memory.ullAvailPhys);

            snapshot.system_used_phys_bytes =
                snapshot.system_total_phys_bytes -
                snapshot.system_avail_phys_bytes;

            snapshot.system_total_pagefile_bytes =
                static_cast<uint64_t>(global_memory.ullTotalPageFile);

            snapshot.system_avail_pagefile_bytes =
                static_cast<uint64_t>(global_memory.ullAvailPageFile);

            snapshot.system_used_pagefile_bytes =
                snapshot.system_total_pagefile_bytes -
                snapshot.system_avail_pagefile_bytes;

            snapshot.system_total_virtual_bytes =
                static_cast<uint64_t>(global_memory.ullTotalVirtual);

            snapshot.system_avail_virtual_bytes =
                static_cast<uint64_t>(global_memory.ullAvailVirtual);

            snapshot.system_used_virtual_bytes =
                snapshot.system_total_virtual_bytes -
                snapshot.system_avail_virtual_bytes;
        }

        PERFORMANCE_INFORMATION performance_info;
        performance_info.cb = sizeof(performance_info);

        if (GetPerformanceInfo(&performance_info, sizeof(performance_info))) {
            snapshot.performance_ok = true;

            const SIZE_T page_size = performance_info.PageSize;

            snapshot.performance_commit_total_bytes =
                cnr3_memory_pages_to_bytes(
                    performance_info.CommitTotal,
                    page_size
                );

            snapshot.performance_commit_limit_bytes =
                cnr3_memory_pages_to_bytes(
                    performance_info.CommitLimit,
                    page_size
                );

            snapshot.performance_commit_peak_bytes =
                cnr3_memory_pages_to_bytes(
                    performance_info.CommitPeak,
                    page_size
                );

            snapshot.performance_physical_total_bytes =
                cnr3_memory_pages_to_bytes(
                    performance_info.PhysicalTotal,
                    page_size
                );

            snapshot.performance_physical_available_bytes =
                cnr3_memory_pages_to_bytes(
                    performance_info.PhysicalAvailable,
                    page_size
                );

            snapshot.performance_physical_used_bytes =
                snapshot.performance_physical_total_bytes -
                snapshot.performance_physical_available_bytes;

            snapshot.performance_system_cache_bytes =
                cnr3_memory_pages_to_bytes(
                    performance_info.SystemCache,
                    page_size
                );

            snapshot.performance_kernel_total_bytes =
                cnr3_memory_pages_to_bytes(
                    performance_info.KernelTotal,
                    page_size
                );

            snapshot.performance_kernel_paged_bytes =
                cnr3_memory_pages_to_bytes(
                    performance_info.KernelPaged,
                    page_size
                );

            snapshot.performance_kernel_nonpaged_bytes =
                cnr3_memory_pages_to_bytes(
                    performance_info.KernelNonpaged,
                    page_size
                );
        }

        return (
            snapshot.process_ok ||
            snapshot.global_ok ||
            snapshot.performance_ok
            );
    }
}

void cnr3_memory_accumulate_snapshot(
    Cnr3MemoryStats& stats,
    const Cnr3MemorySnapshot& snapshot
) {
    if constexpr (!CNR3_MEMORY_DIAGNOSTICS) {
        (void)stats;
        (void)snapshot;
        return;
    }
    else {
        ++stats.sample_count;

        if (snapshot.process_ok) {
            cnr3_memory_update_min_max_sum(
                stats.have_process_working_set,
                stats.process_working_set_sample_count,
                stats.process_working_set_min_bytes,
                stats.process_working_set_max_bytes,
                stats.process_working_set_sum_bytes,
                snapshot.process_working_set_bytes
            );

            cnr3_memory_update_min_max_sum(
                stats.have_process_private_usage,
                stats.process_private_usage_sample_count,
                stats.process_private_usage_min_bytes,
                stats.process_private_usage_max_bytes,
                stats.process_private_usage_sum_bytes,
                snapshot.process_private_usage_bytes
            );
        }

        if (snapshot.global_ok) {
            cnr3_memory_update_min_max_sum(
                stats.have_system_avail_phys,
                stats.system_avail_phys_sample_count,
                stats.system_avail_phys_min_bytes,
                stats.system_avail_phys_max_bytes,
                stats.system_avail_phys_sum_bytes,
                snapshot.system_avail_phys_bytes
            );

            cnr3_memory_update_min_max_sum(
                stats.have_system_used_phys,
                stats.system_used_phys_sample_count,
                stats.system_used_phys_min_bytes,
                stats.system_used_phys_max_bytes,
                stats.system_used_phys_sum_bytes,
                snapshot.system_used_phys_bytes
            );
        }

        if (snapshot.performance_ok) {
            cnr3_memory_update_min_max_sum(
                stats.have_commit_total,
                stats.commit_total_sample_count,
                stats.commit_total_min_bytes,
                stats.commit_total_max_bytes,
                stats.commit_total_sum_bytes,
                snapshot.performance_commit_total_bytes
            );
        }
    }
}


static void cnr3_memory_store_baseline_if_needed(
    Cnr3MemoryStats& stats,
    const Cnr3MemorySnapshot& snapshot
) {
    if (stats.baseline_valid) {
        return;
    }

    stats.baseline_valid = true;
    stats.baseline_working_set_bytes = snapshot.process_working_set_bytes;
    stats.baseline_private_usage_bytes = snapshot.process_private_usage_bytes;
    stats.baseline_avail_phys_bytes = snapshot.system_avail_phys_bytes;
    stats.baseline_used_phys_bytes = snapshot.system_used_phys_bytes;
    stats.baseline_commit_total_bytes = snapshot.performance_commit_total_bytes;
}

static double cnr3_memory_delta_percent(
    double current_mb,
    double baseline_mb
) {
    if (baseline_mb <= 0.0) {
        return 0.0;
    }

    return ((current_mb - baseline_mb) / baseline_mb) * 100.0;
}

static void cnr3_memory_print_legend()
{
    std::fprintf(
        stderr,
        "  Legend:\n"
        "  process_working_set    RAM actively mapped to this process; drops after cache release; persistent delta above baseline suggests leak.\n"
        "  process_private_usage  Best process-level memory-growth indicator; should broadly correlate with cache growth but is not cache-only.\n"
        "  system_avail_phys      Free physical RAM system-wide; falls as the process/system uses more; small percent change is normal.\n"
        "  system_used_phys       Physical RAM in use system-wide; mirror of avail_phys; helps confirm system-level impact.\n"
        "  commit_total           Total committed virtual memory system-wide; can grow with cache and should mostly recover after cleanup.\n"
        "  peak_working_set       Highest working_set seen this run; reveals worst-case RAM pressure from processing.\n"
        "  peak_private_usage     Highest private committed memory seen this run; compare with after-cleanup value.\n"
        "  Min->Max (%%)           Percentage spread from minimum to maximum sample; shows movement during the run, not proof of a leak.\n"
    );
}

static void cnr3_memory_print_snapshot_row(
    const char* metric_name,
    uint64_t current_bytes,
    uint64_t baseline_bytes
) {
    const double current_mb = cnr3_memory_bytes_to_mb(current_bytes);
    const double baseline_mb = cnr3_memory_bytes_to_mb(baseline_bytes);
    const double delta_mb = current_mb - baseline_mb;
    const double delta_percent = cnr3_memory_delta_percent(
        current_mb,
        baseline_mb
    );

    std::fprintf(
        stderr,
        "  %-24s %10.2f %10.2f %+11.2f %+11.2f\n",
        metric_name,
        current_mb,
        baseline_mb,
        delta_mb,
        delta_percent
    );
}

static void cnr3_memory_print_formatted_snapshot(
    const Cnr3MemoryStats& stats,
    const Cnr3MemorySnapshot& snapshot,
    int instance_id,
    const char* label,
    bool show_legend
) {
    std::fprintf(
        stderr,
        "CNR3 memory: instance=%d, %s\n"
        "  %-24s %10s %10s %11s %11s\n",
        instance_id,
        label,
        "Metric",
        "Now (MB)",
        "Start (MB)",
        "Delta (MB)",
        "Delta (%)"
    );

    cnr3_memory_print_snapshot_row(
        "process_working_set",
        snapshot.process_working_set_bytes,
        stats.baseline_working_set_bytes
    );

    cnr3_memory_print_snapshot_row(
        "process_private_usage",
        snapshot.process_private_usage_bytes,
        stats.baseline_private_usage_bytes
    );

    cnr3_memory_print_snapshot_row(
        "system_avail_phys",
        snapshot.system_avail_phys_bytes,
        stats.baseline_avail_phys_bytes
    );

    cnr3_memory_print_snapshot_row(
        "system_used_phys",
        snapshot.system_used_phys_bytes,
        stats.baseline_used_phys_bytes
    );

    cnr3_memory_print_snapshot_row(
        "commit_total",
        snapshot.performance_commit_total_bytes,
        stats.baseline_commit_total_bytes
    );

    std::fprintf(
        stderr,
        "  %-24s %10.2f   (cumulative peak, no delta)\n"
        "  %-24s %10.2f   (cumulative peak, no delta)\n",
        "peak_working_set",
        cnr3_memory_bytes_to_mb(snapshot.process_peak_working_set_bytes),
        "peak_private_usage",
        cnr3_memory_bytes_to_mb(snapshot.process_peak_pagefile_usage_bytes)
    );

    if (show_legend) {
        cnr3_memory_print_legend();
    }
}

void cnr3_memory_record_and_print_snapshot(
    Cnr3MemoryStats& stats,
    bool debug_enabled,
    int instance_id,
    const char* where,
    bool show_legend
) {
    if constexpr (!CNR3_MEMORY_DIAGNOSTICS) {
        (void)stats;
        (void)debug_enabled;
        (void)instance_id;
        (void)where;
        (void)show_legend;
        return;
    }
    else {
        if (!debug_enabled || where == nullptr) {
            return;
        }

        Cnr3MemorySnapshot snapshot;

        if (!cnr3_memory_take_snapshot(snapshot)) {
            std::fprintf(
                stderr,
                "CNR3 memory: instance=%d, %s: snapshot unavailable.\n",
                instance_id,
                where
            );
            std::fflush(stderr);
            return;
        }

        cnr3_memory_store_baseline_if_needed(stats, snapshot);
        cnr3_memory_accumulate_snapshot(stats, snapshot);
        cnr3_memory_print_formatted_snapshot(
            stats,
            snapshot,
            instance_id,
            where,
            show_legend
        );

        std::fflush(stderr);
    }
}

static double cnr3_memory_min_to_max_percent(
    double min_mb,
    double max_mb
) {
    if (min_mb <= 0.0) {
        return 0.0;
    }

    return ((max_mb - min_mb) / min_mb) * 100.0;
}

static void cnr3_memory_print_summary_row(
    const char* metric_name,
    bool have_value,
    uint64_t min_bytes,
    long double sum_bytes,
    uint64_t sample_count,
    uint64_t max_bytes
) {
    const double min_mb = have_value ? cnr3_memory_bytes_to_mb(min_bytes) : 0.0;
    const double avg_mb = have_value
        ? cnr3_memory_average_bytes_to_mb(sum_bytes, sample_count)
        : 0.0;
    const double max_mb = have_value ? cnr3_memory_bytes_to_mb(max_bytes) : 0.0;
    const double min_to_max_percent = have_value
        ? cnr3_memory_min_to_max_percent(min_mb, max_mb)
        : 0.0;

    std::fprintf(
        stderr,
        "  %-28s %10.2f %10.2f %10.2f %+13.2f\n",
        metric_name,
        min_mb,
        avg_mb,
        max_mb,
        min_to_max_percent
    );
}

void cnr3_memory_print_summary(
    const Cnr3MemoryStats& stats,
    bool debug_enabled,
    int instance_id,
    const char* where
) {
    if constexpr (!CNR3_MEMORY_DIAGNOSTICS) {
        (void)stats;
        (void)debug_enabled;
        (void)instance_id;
        (void)where;
        return;
    }
    else {
        if (!debug_enabled || where == nullptr) {
            return;
        }

        if (stats.sample_count == 0) {
            std::fprintf(
                stderr,
                "CNR3 memory: instance=%d, summary (0 samples)\n",
                instance_id
            );
            std::fflush(stderr);
            return;
        }

        std::fprintf(
            stderr,
            "CNR3 memory: instance=%d, summary (%llu samples)\n"
            "  %-28s %10s %10s %10s %13s\n",
            instance_id,
            static_cast<unsigned long long>(stats.sample_count),
            "Metric",
            "Min (MB)",
            "Avg (MB)",
            "Max (MB)",
            "Min->Max (%)"
        );

        cnr3_memory_print_summary_row(
            "process_working_set",
            stats.have_process_working_set,
            stats.process_working_set_min_bytes,
            stats.process_working_set_sum_bytes,
            stats.process_working_set_sample_count,
            stats.process_working_set_max_bytes
        );

        cnr3_memory_print_summary_row(
            "process_private_usage",
            stats.have_process_private_usage,
            stats.process_private_usage_min_bytes,
            stats.process_private_usage_sum_bytes,
            stats.process_private_usage_sample_count,
            stats.process_private_usage_max_bytes
        );

        cnr3_memory_print_summary_row(
            "system_avail_phys",
            stats.have_system_avail_phys,
            stats.system_avail_phys_min_bytes,
            stats.system_avail_phys_sum_bytes,
            stats.system_avail_phys_sample_count,
            stats.system_avail_phys_max_bytes
        );

        cnr3_memory_print_summary_row(
            "system_used_phys",
            stats.have_system_used_phys,
            stats.system_used_phys_min_bytes,
            stats.system_used_phys_sum_bytes,
            stats.system_used_phys_sample_count,
            stats.system_used_phys_max_bytes
        );

        cnr3_memory_print_summary_row(
            "commit_total",
            stats.have_commit_total,
            stats.commit_total_min_bytes,
            stats.commit_total_sum_bytes,
            stats.commit_total_sample_count,
            stats.commit_total_max_bytes
        );

        cnr3_memory_print_legend();
        std::fflush(stderr);
    }
}
