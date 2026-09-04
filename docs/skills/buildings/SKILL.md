# Settlements and buildings

Use `build_buildings` to raise a batch of procedural lots in one call: houses, shops, apartments, offices, and skyscrapers from `generate_procedural_building`, each with per-step evidence. Combine with `roads` for access and `landscape` for terrain.

## Workflow

1. Load this skill (`get_skills` with `buildings`) plus `landscape`/`roads` as needed.
2. Lay out lots as `buildings[]`: each needs a `location` (world coords; Z snaps to the landscape by default), `buildingType` (`house`, `shop`, `apartment`, `office`, `skyscraper`), `width`/`depth`, and `floors`/`floorHeight`.
3. Name lots (`buildingName`) or accept `projectName_Block_N` defaults; vary `seed` per lot (the recipe offsets seeds automatically).
4. Pass `roadSplineActor` + `roadClearance` so lots respect nearby roads; override materials per lot (`wallMaterial`, `roofMaterial`, `windowMaterial`) only with real material paths.
5. Read the summary: per-building success plus actor names. Failures are isolated — other lots still build.

## Iteration loop

Re-run single lots with `generate_procedural_building` for tweaks, or re-run the whole project (names are stable). Stretch a lot by raising `floors`; change character with `buildingType` + roof/material swaps. Clear lots with `build_environment` `delete` by actor name.

## Pitfalls

- Every lot requires `location`; missing locations stack buildings at the origin.
- Lots need a saved `/Game` map like all world recipes.
- Tall types on steep terrain may intersect slopes; flatten pads first with the brush or `build_road` junctions.
- There is no rotation parameter on lots; orient streets to the lot grid instead.
