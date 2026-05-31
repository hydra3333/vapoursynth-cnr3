#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "cnr3_build_config.h"
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

void cnr3_memory_record_and_print_snapshot(
    Cnr3MemoryStats& stats,
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

        Cnr3MemorySnapshot snapshot;

        if (!cnr3_memory_take_snapshot(snapshot)) {
            std::fprintf(
                stderr,
                "CNR3 debug: instance=%d, %s: memory snapshot unavailable.\n",
                instance_id,
                where
            );
            std::fflush(stderr);
            return;
        }

        cnr3_memory_accumulate_snapshot(stats, snapshot);

        std::fprintf(
            stderr,
            "CNR3 debug: instance=%d, %s: memory snapshot: "
            "process_ok=%d, working_set_mb=%.2f, peak_working_set_mb=%.2f, "
            "private_usage_mb=%.2f, pagefile_usage_mb=%.2f, "
            "peak_pagefile_usage_mb=%.2f, "
            "global_ok=%d, memory_load_percent=%u, "
            "system_total_phys_mb=%.2f, system_avail_phys_mb=%.2f, "
            "system_used_phys_mb=%.2f, "
            "system_total_pagefile_mb=%.2f, system_avail_pagefile_mb=%.2f, "
            "system_used_pagefile_mb=%.2f, "
            "system_total_virtual_mb=%.2f, system_avail_virtual_mb=%.2f, "
            "system_used_virtual_mb=%.2f, "
            "performance_ok=%d, commit_total_mb=%.2f, commit_limit_mb=%.2f, "
            "commit_peak_mb=%.2f, physical_total_mb=%.2f, "
            "physical_available_mb=%.2f, physical_used_mb=%.2f, "
            "system_cache_mb=%.2f, kernel_total_mb=%.2f, "
            "kernel_paged_mb=%.2f, kernel_nonpaged_mb=%.2f\n",
            instance_id,
            where,
            snapshot.process_ok ? 1 : 0,
            cnr3_memory_bytes_to_mb(snapshot.process_working_set_bytes),
            cnr3_memory_bytes_to_mb(snapshot.process_peak_working_set_bytes),
            cnr3_memory_bytes_to_mb(snapshot.process_private_usage_bytes),
            cnr3_memory_bytes_to_mb(snapshot.process_pagefile_usage_bytes),
            cnr3_memory_bytes_to_mb(snapshot.process_peak_pagefile_usage_bytes),
            snapshot.global_ok ? 1 : 0,
            snapshot.system_memory_load_percent,
            cnr3_memory_bytes_to_mb(snapshot.system_total_phys_bytes),
            cnr3_memory_bytes_to_mb(snapshot.system_avail_phys_bytes),
            cnr3_memory_bytes_to_mb(snapshot.system_used_phys_bytes),
            cnr3_memory_bytes_to_mb(snapshot.system_total_pagefile_bytes),
            cnr3_memory_bytes_to_mb(snapshot.system_avail_pagefile_bytes),
            cnr3_memory_bytes_to_mb(snapshot.system_used_pagefile_bytes),
            cnr3_memory_bytes_to_mb(snapshot.system_total_virtual_bytes),
            cnr3_memory_bytes_to_mb(snapshot.system_avail_virtual_bytes),
            cnr3_memory_bytes_to_mb(snapshot.system_used_virtual_bytes),
            snapshot.performance_ok ? 1 : 0,
            cnr3_memory_bytes_to_mb(snapshot.performance_commit_total_bytes),
            cnr3_memory_bytes_to_mb(snapshot.performance_commit_limit_bytes),
            cnr3_memory_bytes_to_mb(snapshot.performance_commit_peak_bytes),
            cnr3_memory_bytes_to_mb(snapshot.performance_physical_total_bytes),
            cnr3_memory_bytes_to_mb(snapshot.performance_physical_available_bytes),
            cnr3_memory_bytes_to_mb(snapshot.performance_physical_used_bytes),
            cnr3_memory_bytes_to_mb(snapshot.performance_system_cache_bytes),
            cnr3_memory_bytes_to_mb(snapshot.performance_kernel_total_bytes),
            cnr3_memory_bytes_to_mb(snapshot.performance_kernel_paged_bytes),
            cnr3_memory_bytes_to_mb(snapshot.performance_kernel_nonpaged_bytes)
        );

        std::fflush(stderr);
    }
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
                "CNR3 debug: instance=%d, %s: memory summary: samples=0\n",
                instance_id,
                where
            );
            std::fflush(stderr);
            return;
        }

        std::fprintf(
            stderr,
            "CNR3 debug: instance=%d, %s: memory summary: samples=%llu, "
            "metric_samples: working_set=%llu, private_usage=%llu, "
            "avail_phys=%llu, used_phys=%llu, commit_total=%llu, "
            "process_working_set_mb min/avg/max=%.2f/%.2f/%.2f, "
            "process_private_usage_mb min/avg/max=%.2f/%.2f/%.2f, "
            "system_avail_phys_mb min/avg/max=%.2f/%.2f/%.2f, "
            "system_used_phys_mb min/avg/max=%.2f/%.2f/%.2f, "
            "commit_total_mb min/avg/max=%.2f/%.2f/%.2f\n",
            instance_id,
            where,
            static_cast<unsigned long long>(stats.sample_count),
            static_cast<unsigned long long>(stats.process_working_set_sample_count),
            static_cast<unsigned long long>(stats.process_private_usage_sample_count),
            static_cast<unsigned long long>(stats.system_avail_phys_sample_count),
            static_cast<unsigned long long>(stats.system_used_phys_sample_count),
            static_cast<unsigned long long>(stats.commit_total_sample_count),
            stats.have_process_working_set
            ? cnr3_memory_bytes_to_mb(stats.process_working_set_min_bytes)
            : 0.0,
            stats.have_process_working_set
            ? cnr3_memory_average_bytes_to_mb(
                stats.process_working_set_sum_bytes,
                stats.process_working_set_sample_count
            )
            : 0.0,
            stats.have_process_working_set
            ? cnr3_memory_bytes_to_mb(stats.process_working_set_max_bytes)
            : 0.0,
            stats.have_process_private_usage
            ? cnr3_memory_bytes_to_mb(stats.process_private_usage_min_bytes)
            : 0.0,
            stats.have_process_private_usage
            ? cnr3_memory_average_bytes_to_mb(
                stats.process_private_usage_sum_bytes,
                stats.process_private_usage_sample_count
            )
            : 0.0,
            stats.have_process_private_usage
            ? cnr3_memory_bytes_to_mb(stats.process_private_usage_max_bytes)
            : 0.0,
            stats.have_system_avail_phys
            ? cnr3_memory_bytes_to_mb(stats.system_avail_phys_min_bytes)
            : 0.0,
            stats.have_system_avail_phys
            ? cnr3_memory_average_bytes_to_mb(
                stats.system_avail_phys_sum_bytes,
                stats.system_avail_phys_sample_count
            )
            : 0.0,
            stats.have_system_avail_phys
            ? cnr3_memory_bytes_to_mb(stats.system_avail_phys_max_bytes)
            : 0.0,
            stats.have_system_used_phys
            ? cnr3_memory_bytes_to_mb(stats.system_used_phys_min_bytes)
            : 0.0,
            stats.have_system_used_phys
            ? cnr3_memory_average_bytes_to_mb(
                stats.system_used_phys_sum_bytes,
                stats.system_used_phys_sample_count
            )
            : 0.0,
            stats.have_system_used_phys
            ? cnr3_memory_bytes_to_mb(stats.system_used_phys_max_bytes)
            : 0.0,

            stats.have_commit_total
            ? cnr3_memory_bytes_to_mb(stats.commit_total_min_bytes)
            : 0.0,
            stats.have_commit_total
            ? cnr3_memory_average_bytes_to_mb(
                stats.commit_total_sum_bytes,
                stats.commit_total_sample_count
            )
            : 0.0,
            stats.have_commit_total
            ? cnr3_memory_bytes_to_mb(stats.commit_total_max_bytes)
            : 0.0
        );

        std::fflush(stderr);
    }
}
