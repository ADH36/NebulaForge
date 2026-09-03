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

