import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { describe, expect, it } from 'vitest';
import { addGameplayTag, listGameplayTags, removeGameplayTag } from './gameplay-tags-service.js';

describe('gameplay-tags-service', () => {
  it('adds, lists, deduplicates, and removes project Gameplay Tags', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-tags-'));
    try {
      const added = await addGameplayTag(root, 'Items.Weapon.Rifle', 'A rifle item');
      expect(added).toMatchObject({ success: true, changed: true, tag: 'Items.Weapon.Rifle' });
      expect(await addGameplayTag(root, 'Items.Weapon.Rifle')).toMatchObject({ success: true, changed: false });
      expect(await listGameplayTags(root)).toMatchObject({ count: 1, tags: [{ tag: 'Items.Weapon.Rifle', comment: 'A rifle item' }] });
      expect(await removeGameplayTag(root, 'Items.Weapon.Rifle')).toMatchObject({ success: true, changed: true });
      expect(await listGameplayTags(root)).toMatchObject({ count: 0 });
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });

  it('rejects malformed tags and comments', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-tags-'));
    try {
      await expect(addGameplayTag(root, 'Items/Weapon', '')).rejects.toThrow(/tag must/);
      await expect(addGameplayTag(root, 'Items.Weapon', 'bad\ncomment')).rejects.toThrow(/newlines/);
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });
});
