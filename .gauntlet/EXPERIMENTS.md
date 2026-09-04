# Experiments

## Baseline microbenchmarks

- Custom pool allocator: 12.478 ms versus 65.233 ms for new/delete in the existing single-run benchmark.
- SoA transform loop: 18.103 ms versus 81.646 ms for AoS in the existing single-run benchmark.

These results validate only their isolated benchmark loops. They do not prove that replacing runtime containers or layouts will improve real frame time.

## Proposed measurement-first experiments

### Render preparation arena / persistent batches

- Baseline required: CPU render-preparation time, allocations, instance upload bytes, average/P95/P99.
- Risk: stale batches and incorrect resource invalidation.

### Cached physics proxies or spatial index

- Baseline required: collider counts, proxy build time, query counts, dynamic mutation rate, P95/P99 physics time.
- Risk: stale bounds, nondeterministic ordering, and higher mutation cost.

### GPU timestamp queries

- Baseline required: shadow pass, normal pass, and total GPU duration.
- Risk: query synchronization influencing the timings being measured.

No experiment may replace production behavior without an isolated prototype, comparison, and adversarial review.

