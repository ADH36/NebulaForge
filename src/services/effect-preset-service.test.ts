import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { describe, expect, it } from 'vitest';
import { createEffectPreset, loadEffectPreset } from './effect-preset-service.js';

describe('effect preset service', () => {
  it('writes and validates a project-confined preset', async () => {
    const project = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-effect-preset-'));
    const result = await createEffectPreset({ projectPath: project, presetPath: 'Config/Effects/impact.json', name: 'Impact', actions: [{ action: 'create_impact_effect', args: { intensity: 2 } }] });
    expect(result).toMatchObject({ success: true, name: 'Impact', actionCount: 1 });
    await expect(loadEffectPreset({ projectPath: project, presetPath: 'Config/Effects/impact.json' })).resolves.toMatchObject({ version: 1, name: 'Impact' });
  });

  it('rejects recursive preset orchestration and unsafe locations', async () => {
    await expect(createEffectPreset({ projectPath: 'unused', presetPath: 'Config/Effects/recursive.json', name: 'Recursive', actions: [{ action: 'apply_effect_preset', args: {} }] })).rejects.toThrow('cannot invoke');
    await expect(createEffectPreset({ projectPath: 'unused', presetPath: 'Config/Other/recursive.json', name: 'Recursive', actions: [{ action: 'particle', args: {} }] })).rejects.toThrow('under Config/Effects');
  });
});
