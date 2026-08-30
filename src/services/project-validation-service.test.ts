import { describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { validateProject } from './project-validation-service.js';

async function fixture(): Promise<string> {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-project-'));
  await fs.mkdir(path.join(root, 'Content'));
  await fs.mkdir(path.join(root, 'Config'));
  await fs.writeFile(path.join(root, 'Game.uproject'), JSON.stringify({ FileVersion: 3, Modules: [], Plugins: [] }));
  await fs.writeFile(path.join(root, 'Config', 'DefaultGame.ini'), '[/Script/EngineSettings.GeneralProjectSettings]\n');
  return root;
}

describe('validateProject', () => {
  it('validates a structurally sound Unreal project', async () => {
    const root = await fixture();
    const result = await validateProject({ projectPath: root, requiredFiles: ['Config/DefaultGame.ini'] });
    expect(result.success).toBe(true);
    expect(result.projectFile).toBe('Game.uproject');
  });

  it('reports missing required project files and rejects traversal', async () => {
    const root = await fixture();
    const result = await validateProject({ projectPath: root, requiredFiles: ['../outside.txt', 'Config/Missing.ini'] });
    expect(result.success).toBe(false);
    expect(result.failedChecks).toEqual(expect.arrayContaining(['required_file']));
  });
});
