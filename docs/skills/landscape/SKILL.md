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

For roads and rivers, use `build_road` (or `build_river`): terrain-conformed
spline, cut/fill corridor, roadbed segments from your mesh, furniture scatter
with lateral offsets for guardrails/lamps/markings, junction discs, and river
water following the spline. Same seed regenerates deterministically.

