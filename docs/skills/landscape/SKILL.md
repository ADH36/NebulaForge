# Landscape workflows

Use `build_environment` for landscape creation, heightmaps, edit layers, landscape splines,
foliage, and terrain material setup. Inspect the current landscape and enabled plugins before
writing. Match imported heightmap resolution to the landscape component layout, use the project
safe-save path, and verify persistence after saving or reloading.

For full worlds, prefer `generate_world` (or `apply_biome` with a `create_biome_preset` asset):
one call orchestrates landscape + seeded terrain (+erosion) + rule-based layer paint
(height/slope/altitude/noise masks) + deterministic HISM foliage, and returns per-step
PASS/PARTIAL/FAIL evidence. Same seed and preset regenerate deterministically; pass
`reuseExistingLandscape: true` to rebuild terrain on an existing landscape.

For interactive work, use the World Brush editor mode (Modes panel, or the Brush
section of the Generate World tab): Raise, Lower, Flatten, and Smooth height
strokes, additive layer painting, and accumulating HISM foliage scatter, all with
radius/strength/falloff controls. Programmatic single dabs stay available through
`sculpt_landscape` (toolMode Raise/Lower/Flatten/Smooth), `paint_landscape_layer`,
and `scatter_landscape_foliage`.

## Iteration loop (generate → measure → adjust)

1. Generate with `generate_world` (inline params) or `apply_biome` (preset + overrides).
2. Read the combined summary: `status` PASS/PARTIAL/FAIL plus per-step evidence. PARTIAL names the failed step — fix that step's inputs, not the whole recipe.
3. Measure with `inspect_landscape` and `inspect_generated_foliage`; adjust rules and re-run with the same `seed` (deterministic) and `reuseExistingLandscape: true` to rebuild on the same actor.
4. Typical adjustments: rock missing → widen its slope band; snow everywhere → raise `minHeight`; bald spots → raise `count` or lower `maxSlope`; layers invisible → check the auto material was created and layer infos exist.

## Biome presets

Author reusable presets with `create_biome_preset` (same grammar as `generate_world`), inspect them with `inspect_biome_preset`, list them with `list_biome_presets`, and rebuild them with `apply_biome`. Presets are `UMcpBiomePreset` assets and editable in the content browser. Pair with the `forest`, `roads`, `buildings`, and `water` skills for full worlds.

For roads and rivers, use `build_road` (or `build_river`): terrain-conformed
spline, cut/fill corridor, roadbed segments from your mesh, furniture scatter
with lateral offsets for guardrails/lamps/markings, junction discs, and river
water following the spline. Same seed regenerates deterministically.

