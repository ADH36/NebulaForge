# Roads, rivers, and paths

Use `build_environment` road recipes for spline-based transport: `build_road` for roads/paths, `build_river` for rivers. One call runs the full pipeline — terrain-conformed spline, cut/fill corridor, roadbed mesh segments, lateral-offset furniture (guardrails, lamps, center-line markings), junction discs, and river water following the spline — and returns per-step PASS/PARTIAL/FAIL evidence.

## Workflow

1. Load this skill (`get_skills` with `roads`) plus `landscape` when terrain exists.
2. Build the route from 2+ world-space `routePoints`. Keep segments longer than the road width and grade changes gentle; set `maxSlopeDegrees` to reject impossible grades.
3. Call `build_road` with `roadKind` (`road`, `river`, `path`), `roadWidth`, `shoulderWidth`, and `cutFill: true` so terrain is flattened toward the profile.
4. Supply `roadbedMeshPath` (oriented along `forwardAxis`, default X) plus `roadbedMaterialPath` for the driving surface.
5. Scatter furniture with lateral `offset` (positive = right of travel): guardrails at half-width + margin with `bothSides: true`, lamps every N meters on one side, dashes at offset 0 with small spacing.
6. Declare `junctions` with world `location` + `radius`; terrain is flattened and an optional `junctionMeshPath` spawns at the leveled height.
7. For rivers, `build_river` enables water automatically; pass `skipWater: true` only when the Water plugin is unavailable.

## Iteration loop

Read the combined summary: `status` PASS means done; PARTIAL names the failed step. Common repairs: spline step fails → check point count (min 2), duplicates, and `/Game` saved map; cut/fill fails → no landscape under the route (`landscapeName` or generate terrain first); furniture places zero → mesh path invalid or spacing larger than spline length; water fails → enable the Water plugin or `skipWater: true`.

## Pitfalls

- Route points are world coordinates; set `coordinateSpace` handling via the recipe (World is default).
- `conformToLandscape` is enabled by the recipe; residuals are removed by cut/fill, so small terrain bumps are fine.
- Furniture needs real static-mesh paths; engine `/Engine/BasicShapes/*` meshes work for smoke tests.
- Long roads run many flatten dabs; keep `shoulderWidth` tight on first passes.
