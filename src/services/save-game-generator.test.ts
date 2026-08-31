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

  it('generates a versioned schema field and migration manifest', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-savegame-versioned-'));
    try {
      const result = await generateSaveGameClass({
        projectPath: root,
        className: 'USaveProfile',
        headerPath: 'Source/Game/SaveProfile.h',
        sourcePath: 'Source/Game/SaveProfile.cpp',
        schemaVersion: 3,
        migrations: [{ fromVersion: 1, toVersion: 2, description: 'Add inventory capacity' }, { fromVersion: 2, toVersion: 3, description: 'Add difficulty setting' }],
        variables: [{ name: 'Coins', type: 'int32', defaultValue: 10 }]
      });
      expect(result).toMatchObject({ success: true, schemaVersion: 3, migrationManifest: { schemaVersion: 3, migrationCount: 2 } });
      const header = await fs.readFile(path.join(root, 'Source/Game/SaveProfile.h'), 'utf8');
      expect(header).toContain('int32 SaveSchemaVersion = 3;');
      const manifest = JSON.parse(await fs.readFile(path.join(root, 'Config/SaveGame/USaveProfile.schema.json'), 'utf8')) as Record<string, unknown>;
      expect(manifest).toMatchObject({ schemaVersion: 3, className: 'USaveProfile' });
      expect(manifest.migrations).toHaveLength(2);
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });

  it('rejects invalid migration ranges and reserved schema names', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-savegame-invalid-'));
    try {
      await expect(generateSaveGameClass({ className: 'USaveProfile', headerPath: 'Save.h', sourcePath: 'Save.cpp', schemaVersion: 2, migrations: [{ fromVersion: 2, toVersion: 1, description: 'invalid' }], variables: [] })).rejects.toThrow(/migration/);
      await expect(generateSaveGameClass({ className: 'USaveProfile', headerPath: 'Save.h', sourcePath: 'Save.cpp', variables: [{ name: 'SaveSchemaVersion', type: 'int32' }] })).rejects.toThrow(/reserved/);
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });
});
