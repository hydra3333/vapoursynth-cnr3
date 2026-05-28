#include "cnr3_response_tables.h"

#include <cmath>

int get_cnr3_table_value_for_signed_diff(
    const std::vector<int> &table,
    int table_offset,
    int signed_diff
) {
    /*
        Safe table lookup helper.

        The real blend path uses current-vs-previous signed sample differences:

            signed_diff = current_sample - previous_sample

        The table is stored with a positive offset so signed differences can be
        used directly after adding table_offset.
    */
    const int index = signed_diff + table_offset;

    if (index < 0 || index >= static_cast<int>(table.size())) {
        return 0;
    }

    return table[static_cast<size_t>(index)];
}

void build_cnr3_weight_table(
    std::vector<int> &table,
    int table_offset,
    int table_size,
    int sample_peak,
    int threshold,
    int strength,
    bool wide_response
) {
    /*
        Build one vscnr2-style signed-difference response table.

        This replaces the earlier temporary enabled/disabled scaffold.

        Important:
            mode character 'x' means narrow response, not disabled.
            mode character 'o' means wide response, not enabled.

        A response value near strength means:
            the current-vs-previous difference is small enough that the
            recursive chroma blend may strongly reuse previous filtered chroma.

        A response value near zero means:
            the difference is large enough that the recursive chroma blend
            should mostly or entirely keep current source chroma.

        Narrow response:
            The table falls away more quickly as abs(diff) increases.
            This is safer and less aggressive.

        Wide response:
            The table stays higher for longer as abs(diff) increases.
            This is stronger and more tolerant of chroma shimmer, but has more
            risk of chroma lag, smearing, or ghosting around real motion.

        Why default mode="oxx" can still make sense:
            - Y uses wide response so luma structure does not block chroma
              stabilisation too eagerly.
            - U and V use narrow response so actual chroma changes are handled
              more conservatively.
            - This matches the historical default while still making all three
              planes participate in the blend decision.

        Table storage:
            table[signed_diff + table_offset]

        Table value range:
            0..sample_peak

        The table is signed because the vscnr2-style formula uses signed
        current-vs-previous differences when indexing the Y/U/V response
        tables. For cosine response curves the result is symmetric, but keeping
        signed indexing keeps the lookup path aligned with the blend formula.
    */

    table.assign(static_cast<size_t>(table_size), 0);

    threshold = cnr3_clamp_int(threshold, 0, sample_peak);
    strength = cnr3_clamp_int(strength, 0, sample_peak);

    if (threshold == 0) {
        table[static_cast<size_t>(table_offset)] = strength;
        return;
    }

    constexpr double pi = 3.141592653589793238462643383279502884;

    const int first_diff = -threshold;
    const int last_diff = threshold;

    for (int signed_diff = first_diff; signed_diff <= last_diff; ++signed_diff) {
        const int index = signed_diff + table_offset;

        if (index < 0 || index >= table_size) {
            continue;
        }

        const int abs_diff = std::abs(signed_diff);

        double angle = 0.0;

        if (wide_response) {
            /*
                Wide response.

                Squaring abs_diff keeps the curve higher for longer near zero,
                then it falls toward zero near the threshold.
            */
            angle =
                static_cast<double>(abs_diff) *
                static_cast<double>(abs_diff) *
                pi /
                (
                    static_cast<double>(threshold) *
                    static_cast<double>(threshold)
                );
        } else {
            /*
                Narrow response.

                Linear abs_diff makes the curve fall away sooner.
            */
            angle =
                static_cast<double>(abs_diff) *
                pi /
                static_cast<double>(threshold);
        }

        /*
            Use integer division by 2 before applying the cosine response.
            This intentionally follows the vscnr2-style table shape closely,
            including the fact that an odd maximum strength such as 255 gives
            a peak table value of 254 rather than 255.
        */
        const double half_strength =
            static_cast<double>(strength / 2);

        const int value = cnr3_clamp_int(
            static_cast<int>(half_strength * (1.0 + std::cos(angle))),
            0,
            sample_peak
        );

        table[static_cast<size_t>(index)] = value;
    }
}

bool build_cnr3_lookup_tables(
    Cnr3Data &d,
    VSMap *out,
    const VSAPI *vsapi
) {
    /*
        mode is a 3-character string:
            mode[0] controls the luma/Y response curve
            mode[1] controls the U/chroma response curve
            mode[2] controls the V/chroma response curve

        Historical vscnr2/Cnr2-compatible meaning:
            'x' = narrow response curve
            'o' = wide response curve

        Very important:
            'x' does not mean off.
            'o' does not mean on.

        All three planes still get tables. The mode character only changes the
        curve shape used to reduce the later blend weight as
        current-vs-previous differences increase.
    */

    if (d.mode.size() != 3) {
        vsapi->mapSetError(out, "CNR3: internal error: mode must contain exactly 3 characters.");
        return false;
    }

    const bool wide_y = (d.mode[0] != 'x');
    const bool wide_u = (d.mode[1] != 'x');
    const bool wide_v = (d.mode[2] != 'x');

    build_cnr3_weight_table(
        d.table_y,
        d.table_offset,
        d.table_size,
        d.sample_peak,
        d.ln_scaled,
        d.lm_scaled,
        wide_y
    );

    build_cnr3_weight_table(
        d.table_u,
        d.table_offset,
        d.table_size,
        d.sample_peak,
        d.un_scaled,
        d.um_scaled,
        wide_u
    );

    build_cnr3_weight_table(
        d.table_v,
        d.table_offset,
        d.table_size,
        d.sample_peak,
        d.vn_scaled,
        d.vm_scaled,
        wide_v
    );

    if (
        d.table_y.size() != static_cast<size_t>(d.table_size) ||
        d.table_u.size() != static_cast<size_t>(d.table_size) ||
        d.table_v.size() != static_cast<size_t>(d.table_size)
    ) {
        vsapi->mapSetError(out, "CNR3: internal error: lookup-table size mismatch.");
        return false;
    }

    return true;
}
