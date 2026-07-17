# HALF-500 + PlanRetry 50 ms Mode Comparison

Just for this analysis, `Tx` is not denoting threads:

- `T0` = `fmUnordered`
- `T1` = `fmParallelRequests`
- `T2` = `fmParallel`

| Row | Mode | Sleep | Threads | N/2 | Runtime | FPS | Source requested | Frames computed | Duplicates | Holes | Recalc count | Span mean | Span max | Retry sleeps | Plans dumped | Kept holes | Invariants | Ownership |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 50msT0 | fmUnordered | 50 | 24 | 12 | 31.48 | 95.29 | 6148 | 3000 | 0 | 3148 | 0 | 2.570 | 4 | 393 | 393 | 3148 | 0 | 0 |
| 50msT1 | fmParallelRequests | 50 | 24 | 12 | 24.19 | 123.99 | 6653 | 3139 | 139 | 3651 | 139 | 2.218 | 14 | 8949 | 8949 | 3651 | 0 | 0 |
| 50msT2 | fmParallel | 50 | 24 | 12 | 23.79 | 126.12 | 5380 | 5342 | 2342 | 2380 | 2342 | 1.794 | 18 | 8979 | 8979 | 2380 | 0 | 0 |
