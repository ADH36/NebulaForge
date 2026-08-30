import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { describe, expect, it } from 'vitest';
import { generateSaveGameClass } from './save-game-generator.js';

describe('save-game-generator', () => {
  it('generates compile-ready Unreal SaveGame source with allowlisted fields', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-savegame-'));
    try {
      const result = await generateSaveGameClass({
        projectPath: root,
        className: 'USaveProfile',
        headerPath: 'Source/Game/SaveProfile.h',
        sourcePath: 'Source/Game/SaveProfile.cpp',
        variables: [
          { name: 'Coins', type: 'int32', defaultValue: 10 },
          { name: 'PlayerName', type: 'FString', defaultValue: 'Pilot' }
        ]
      });
      expect(result).toMatchObject({ success: true, className: 'USaveProfile', variableCount: 2 });
      const header = await fs.readFile(path.join(root, 'Source/Game/SaveProfile.h'), 'utf8');
      expect(header).toContain('class USaveProfile : public USaveGame');
      expect(header).toContain('int32 Coins = 10;');
      expect(header).toContain('FString PlayerName = TEXT("Pilot");');
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });

  it('rejects unsupported types and unsafe identifiers', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-savegame-'));
    try {
      await expect(generateSaveGameClass({
        projectPath: root,
        className: 'USaveProfile',
        headerPath: 'Save.h',
        sourcePath: 'Save.cpp',
        variables: [{ name: 'Inventory', type: 'TArray<FString>' }]
      })).rejects.toThrow(/Unsupported SaveGame property type/);
      await expect(generateSaveGameClass({
        projectPath: root,
        className: 'SaveProfile',
        headerPath: 'Save.h',
        sourcePath: 'Save.cpp',
        variables: []
      })).rejects.toThrow(/U-prefixed/);
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });
});
