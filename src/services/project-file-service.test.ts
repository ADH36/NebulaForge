import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { describe, expect, it } from 'vitest';
import { readProjectFile, writeProjectFile } from './project-file-service.js';

describe('project-file-service', () => {
  it('writes atomically, creates a backup, and reads project files', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-project-'));
    try {
      await fs.writeFile(path.join(root, 'Game.uproject'), '{}');
      const first = await writeProjectFile(root, 'Config/DefaultGame.ini', '[Game]\nName=One\n');
      expect(first).toMatchObject({ success: true, created: true, filePath: 'Config/DefaultGame.ini' });

      const second = await writeProjectFile(root, 'Config/DefaultGame.ini', '[Game]\nName=Two\n');
      expect(second).toMatchObject({ success: true, created: false, backupPath: 'Config/DefaultGame.ini.bak' });
      expect(await fs.readFile(path.join(root, 'Config/DefaultGame.ini.bak'), 'utf8')).toContain('Name=One');

      const read = await readProjectFile(root, 'Config/DefaultGame.ini');
      expect(read).toMatchObject({ success: true, content: '[Game]\nName=Two\n' });
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });

  it('rejects traversal and protected build-output paths', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-project-'));
    try {
      await expect(writeProjectFile(root, '../outside.ini', 'x')).rejects.toThrow(/parent-directory/);
      await expect(writeProjectFile(root, 'Intermediate/Generated.cpp', 'x')).rejects.toThrow(/protected/);
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });
});
