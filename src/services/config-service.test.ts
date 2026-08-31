import { afterEach, describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { flushConfig, getConfigHierarchy, getConfigValue, listConfigLayers, reloadConfig, setConfigValue } from './config-service.js';

describe('config service', () => {
  let root = '';
  afterEach(async () => { if (root) await fs.rm(root, { recursive: true, force: true }); });

  it('lists layers and updates an ini key atomically', async () => {
    root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-config-'));
    await fs.mkdir(path.join(root, 'Config'), { recursive: true });
    await fs.writeFile(path.join(root, 'Config', 'DefaultGame.ini'), '[/Script/Game.Settings]\nMaxPlayers=4\n', 'utf8');
    expect((await listConfigLayers(root)).count).toBe(1);
    const write = await setConfigValue(root, 'DefaultGame.ini', '/Script/Game.Settings', 'MaxPlayers', '8');
    expect(write.success).toBe(true);
    expect((await getConfigValue(root, 'DefaultGame.ini', '/Script/Game.Settings', 'MaxPlayers')).value).toBe('8');
    await expect(fs.access(path.join(root, 'Config', 'DefaultGame.ini.bak'))).resolves.toBeUndefined();
  });

  it('rejects unsafe config names and multiline values', async () => {
    expect((await setConfigValue(undefined, '../DefaultGame.ini', 'Game', 'Key', 'x')).success).toBe(false);
    expect((await setConfigValue(undefined, 'DefaultGame.ini', 'Game', 'Key', 'x\ny')).success).toBe(false);
  });

  it('reloads, flushes, and inventories platform override layers', async () => {
    root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-config-lifecycle-'));
    await fs.mkdir(path.join(root, 'Config', 'Windows'), { recursive: true });
    await fs.writeFile(path.join(root, 'Config', 'DefaultGame.ini'), '[Game]\nMaxPlayers=4\n', 'utf8');
    await fs.writeFile(path.join(root, 'Config', 'Windows', 'WindowsGame.ini'), '[Game]\nMaxPlayers=8\n', 'utf8');
    const hierarchy = await getConfigHierarchy(root);
    expect(hierarchy).toMatchObject({ success: true, count: 2 });
    expect(hierarchy.layers).toEqual([
      { name: 'DefaultGame.ini', relativePath: 'Config/DefaultGame.ini', scope: 'project_default', priority: 0 },
      { name: 'WindowsGame.ini', relativePath: 'Config/Windows/WindowsGame.ini', scope: 'platform:Windows', priority: 1 }
    ]);
    expect(await reloadConfig(root, 'DefaultGame.ini')).toMatchObject({ success: true, sectionCount: 1, keyCount: 1, reloaded: true });
    const flushed = await flushConfig(root, 'DefaultGame.ini');
    expect(flushed).toMatchObject({ success: true, flushed: true });
    expect(Number(flushed.bytes)).toBeGreaterThan(0);
  });
});
