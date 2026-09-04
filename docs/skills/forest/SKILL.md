# Forests and vegetation

Use `plant_forest` for believable clustered woodland: seeded cluster centers, multi-species mixes by weight, slope/height filtering, and one HISM collection actor per forest (tagged for `inspect_generated_foliage` / `clear_generated_foliage`). Prefer it over uniform `scatter_landscape_foliage` whenever trees should clump.

## Workflow

1. Load this skill (`get_skills` with `forest`) plus `landscape` for terrain context.
2. Ensure a landscape exists (generate one or target it with `landscapeName`).
3. Call `plant_forest` with `species[]` (`meshPath` + `weight` or explicit `count`, `minScale`/`maxScale`), `totalCount`, `clusterCount`, and `clusterRadius`.
4. Constrain with `minSlope`/`maxSlope` (default max 35 keeps trees off cliffs) and `minHeight`/`maxHeight` (keep mangroves low, pines high).
5. Read the summary: per-species `placedCount` vs `targetCount`. Zero placements mean the filters reject everything — widen slope/height bands or raise `totalCount`.

## Iteration loop

Regenerate deterministically with the same `seed`; the collection actor is reused by label (`forestName`). Thin a forest by re-running with a smaller `totalCount`; expand it by adding clusters. Clear with `clear_generated_foliage`.

## Pitfalls

- Species need real static-mesh paths; trunks must be modeled near the mesh origin or trees float/sink.
- `clusterRadius` too large dissolves into uniform scatter; too small starves edge clusters (attempts are bounded per cluster).
- Steep presets (mountains) need explicit `maxSlope` or most samples reject.
- Counts above a few thousand instances are fine for HISM but slow the planting pass; grow big forests in stages.
