import { describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { manageProjectPlugins } from './project-plugin-service.js';

async function pluginFixture(): Promise<string> {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-plugin-'));
  await fs.mkdir(path.join(root, 'Content'));
  await fs.mkdir(path.join(root, 'Config'));
  await fs.writeFile(path.join(root, 'Game.uproject'), JSON.stringify({ FileVersion: 3, Plugins: [{ Name: 'ExamplePlugin', Enabled: false }] }));
  return root;
}

describe('manageProjectPlugins', () => {
  it('lists and toggles declared plugins with a backup', async () => {
    const root = await pluginFixture();
    expect((await manageProjectPlugins(root, 'list')).plugins).toHaveLength(1);
    const result = await manageProjectPlugins(root, 'enable', 'ExamplePlugin', true);
    expect(result.success).toBe(true);
    expect(JSON.parse(await fs.readFile(path.join(root, 'Game.uproject'), 'utf8')).Plugins[0].Enabled).toBe(true);
    await expect(fs.access(path.join(root, 'Game.uproject.bak'))).resolves.toBeUndefined();
  });

  it('refuses undeclared plugins', async () => {
    const root = await pluginFixture();
    const result = await manageProjectPlugins(root, 'enable', 'MissingPlugin');
    expect(result.error).toBe('PLUGIN_NOT_DECLARED');
  });
});
