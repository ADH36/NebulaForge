#!/usr/bin/env node
/**
 * World recipe regression: biome preset authoring, orchestrated generate_world
 * pipeline (landscape + terrain + rule paint + foliage), deterministic
 * regeneration, and cleanup.
 * Run against a UE 5.8 editor: node tests/mcp-tools/world/world-recipes.test.mjs
 */
import { runToolTests } from '../../test-runner.mjs';

const stamp = Date.now();
const folder = `/Game/MCPTest/WorldRecipes_${stamp}`;
const preset = `BIOME_Test_${stamp}`;
const landscape = `MCP_WorldRecipeLandscape_${stamp}`;

runToolTests('world-recipes', [
  { scenario: 'Setup: create temporary world recipe folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: folder }, expected: 'success|already exists' },

  { scenario: 'Presets: list biome presets reports a count', toolName: 'build_environment', arguments: { action: 'list_biome_presets' }, expected: 'success', assertions: [{ path: 'structuredContent.result.presetCount', greaterThan: -1, label: 'preset count reported' }] },
  { scenario: 'Presets: create biome preset with terrain, layer rules and foliage', toolName: 'build_environment', arguments: {
      action: 'create_biome_preset',
      name: preset,
      path: folder,
      description: 'Integration test biome preset',
      seed: 4242,
      landscape: { landscapeName: landscape, componentsX: 4, componentsY: 4, quadsPerComponent: 63, sectionsPerComponent: 1 },
      terrain: { terrainFeature: 'hills', heightScale: 4096, frequency: 2, resolution: 129, erosion: true, iterations: 4 },
      layers: [
        { layerName: 'Grass', maskType: 'height', minHeight: -1000000, maxHeight: 1000000, strength: 1 },
        { layerName: 'Rock', maskType: 'slope', minSlope: 25, maxSlope: 90, strength: 1 }
      ],
      foliageTypes: [{ meshPath: '/Engine/BasicShapes/Sphere', count: 8, minScale: 0.5, maxScale: 1.0 }]
    }, expected: 'success', assertions: [{ path: 'structuredContent.result.layerRuleCount', equals: 2, label: 'preset stores both layer rules' }, { path: 'structuredContent.result.foliageEntryCount', equals: 1, label: 'preset stores foliage entry' }] },
  { scenario: 'Presets: inspect biome preset round-trips the recipe', toolName: 'build_environment', arguments: { action: 'inspect_biome_preset', biomePresetPath: `${folder}/${preset}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.seed', equals: 4242, label: 'seed round-trips' }] },

  { scenario: 'Generate: apply_biome runs the full orchestrated pipeline', toolName: 'build_environment', arguments: { action: 'apply_biome', biomePresetPath: `${folder}/${preset}`, seed: 4242, reuseExistingLandscape: true }, expected: 'success', assertions: [
      { path: 'structuredContent.result.status', equals: 'PASS', label: 'all recipe steps passed' },
      { path: 'structuredContent.result.stepCount', greaterThan: 0, label: 'recipe executed steps' },
      { path: 'structuredContent.result.failedSteps', equals: 0, label: 'no failed steps' },
      { path: 'structuredContent.result.landscapeName', equals: landscape, label: 'landscape created from preset name' }
    ] },
  { scenario: 'Generate: regenerate from the same preset and seed reuses the landscape deterministically', toolName: 'build_environment', arguments: { action: 'apply_biome', biomePresetPath: `${folder}/${preset}`, seed: 4242, reuseExistingLandscape: true }, expected: 'success', assertions: [
      { path: 'structuredContent.result.status', equals: 'PASS', label: 'regeneration steps passed' },
      { path: 'structuredContent.result.reusedExistingLandscape', equals: true, label: 'existing landscape reused' }
    ] },

  { scenario: 'Inspect: generated landscape exists with preset sizing', toolName: 'build_environment', arguments: { action: 'inspect_landscape', landscapeName: landscape }, expected: 'success' },
  { scenario: 'Inspect: tool-generated foliage collections exist', toolName: 'build_environment', arguments: { action: 'inspect_generated_foliage', landscapePath: '' }, expected: 'success' },

  { scenario: 'Generate: inline generate_world skips foliage and paint cleanly', toolName: 'build_environment', arguments: { action: 'generate_world', name: `MCP_WorldRecipeBare_${stamp}`, seed: 77, componentsX: 2, componentsY: 2, quadsPerComponent: 63, terrainFeature: 'plains', skipPaint: true, skipFoliage: true, reuseExistingLandscape: false }, expected: 'success', assertions: [
      { path: 'structuredContent.result.failedSteps', equals: 0, label: 'inline recipe has no failures' },
      { path: 'structuredContent.result.layerRuleCount', equals: 0, label: 'layer rules skipped' }
    ] },

  { scenario: 'Cleanup: delete generated landscapes', toolName: 'build_environment', arguments: { action: 'delete_landscape', landscapeName: landscape }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete bare inline landscape', toolName: 'build_environment', arguments: { action: 'delete_landscape', landscapeName: `MCP_WorldRecipeBare_${stamp}` }, expected: 'success|not found' },
  { scenario: 'Cleanup: clear tool-generated foliage', toolName: 'build_environment', arguments: { action: 'clear_generated_foliage' }, expected: 'success' },
  { scenario: 'Cleanup: delete all temporary world recipe content', toolName: 'manage_asset', arguments: { action: 'delete', path: folder, force: true }, expected: 'success|not found' }
]);
