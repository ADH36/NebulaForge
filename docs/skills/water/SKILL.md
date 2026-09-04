# Lakes and water

Use `build_lake` for basins with water: it carves the terrain toward a bed height, then spawns a `WaterBodyLake` at the resolved surface level. Use `build_river` (see `roads`) for flowing water along a spline. The Water plugin must be enabled; otherwise water steps report `WATER_UNAVAILABLE`.

## Workflow

1. Load this skill (`get_skills` with `water`) plus `landscape` for terrain context.
2. Call `build_lake` with a `location` (basin center), `radius`, and `depth`. Omit `waterLevel` to derive it from the terrain (center height minus half depth), or set it absolutely for linked water systems.
3. Optionally pass `materialPath`/`waterMaterialPath` for the lake surface.
4. Read the summary: `bedZ`, `waterLevel`, carved vertex count. If the basin reads too shallow, raise `depth` and re-run (carving is deterministic from the same center).

## Iteration loop

Shorelines come from the carve falloff; widen beaches with a larger `radius` at the same depth. Chain `plant_forest` with a `minHeight` above the water level for shoreline trees. Remove a lake by deleting its water actor (`build_environment` `delete`) and re-sculpting the basin with the brush.

## Pitfalls

- The map must be saved under `/Game`; carving needs a landscape under the center point.
- Closed basins need surrounding terrain above `waterLevel`, or water reads as a floating plane — check with a top-down screenshot.
- `create_water_body_ocean`/`river`/`custom` remain available for non-lake water via the same `actorName` + `location` grammar.
