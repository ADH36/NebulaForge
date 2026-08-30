import { afterEach, describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { getConfigValue, listConfigLayers, setConfigValue } from './config-service.js';

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
    expect(await fs.access(path.join(root, 'Config', 'DefaultGame.ini.bak'))).resolves.toBeUndefined();
  });

  it('rejects unsafe config names and multiline values', async () => {
    expect((await setConfigValue(undefined, '../DefaultGame.ini', 'Game', 'Key', 'x')).success).toBe(false);
    expect((await setConfigValue(undefined, 'DefaultGame.ini', 'Game', 'Key', 'x\ny')).success).toBe(false);
  });
});
