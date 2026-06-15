#include "cnr3_instance_config.h"

#include <atomic>
#include <climits>

/*
    Per-process diagnostic ID source.

    This is intentionally not cache state and not shared frame-processing state.
    It only gives each created CNR3 instance a small human-readable identifier
    for stderr diagnostics and future proof summaries.

    Relaxed ordering is sufficient because the counter does not publish or
    protect any other data. It is only an ID generator.
*/
namespace {
    std::atomic<int> g_cnr3_next_instance_id{ 1 };
}

Cnr3InstanceId cnr3_allocate_instance_id() noexcept {
    int observed = g_cnr3_next_instance_id.load(std::memory_order_relaxed);

    for (;;) {
        const int returned_id = (observed > 0) ? observed : 1;
        const int next_id = (returned_id < INT_MAX) ? (returned_id + 1) : 1;

        if (g_cnr3_next_instance_id.compare_exchange_weak(
            observed,
            next_id,
            std::memory_order_relaxed,
            std::memory_order_relaxed
        )) {
            return Cnr3InstanceId{ returned_id };
        }

        /*
            compare_exchange_weak updates observed on failure. Loop until this
            activation successfully reserves one diagnostic ID.
        */
    }
}

Cnr3InstanceConfig cnr3_make_default_instance_config() noexcept {
    Cnr3InstanceConfig config{};

    config.instance_id = cnr3_allocate_instance_id();

    return config;
}

bool cnr3_instance_config_is_valid(
    const Cnr3InstanceConfig& config
) noexcept {
    return cnr3_instance_id_is_valid(config.instance_id);
}
