import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { describe, expect, it } from 'vitest';
import { addArchitectureRequirement, createGameArchitectureManifest, validateGameArchitecture } from './game-architecture-service.js';

describe('game architecture service', () => {
  it('creates, extends, and validates a project architecture manifest', async () => {
    const project = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-architecture-'));
    await fs.mkdir(path.join(project, 'Source', 'NebulaGame'), { recursive: true });
    await fs.writeFile(path.join(project, 'Source', 'NebulaGame', 'NebulaGame.Build.cs'), 'module');
    await createGameArchitectureManifest({
      projectPath: project,
      manifestPath: 'Config/Architecture/game.json',
      projectName: 'NebulaGame',
      requirements: [{ id: 'game-module', kind: 'module' }, { id: 'build-file', kind: 'file', path: 'Source/NebulaGame/NebulaGame.Build.cs' }]
    });
    await addArchitectureRequirement({ projectPath: project, manifestPath: 'Config/Architecture/game.json', requirement: { id: 'missing-test', kind: 'test', path: 'Source/NebulaGame/MissingTest.cpp' } });
    await expect(validateGameArchitecture({ projectPath: project, manifestPath: 'Config/Architecture/game.json' })).resolves.toMatchObject({ success: false, missingRequired: ['missing-test'] });
  });

  it('rejects unsafe manifest paths', async () => {
    await expect(createGameArchitectureManifest({ manifestPath: 'Config/../game.json', projectName: 'Game' })).rejects.toThrow('manifestPath');
  });
});
