import { describe, expect, it } from 'vitest';
import { SkillRegistry } from '../../../src/services/skill-registry.js';

describe('SkillRegistry', () => {
  it('discovers the bundled catalog and ignores missing roots', async () => {
    const original = process.env.NEBULAFORGE_SKILLS_PATH;
    process.env.NEBULAFORGE_SKILLS_PATH = 'C:/path-that-does-not-exist';
    await expect(new SkillRegistry().list()).resolves.toEqual(expect.arrayContaining([
      expect.objectContaining({ name: 'landscape' }),
      expect.objectContaining({ name: 'performance' })
    ]));
    if (original === undefined) delete process.env.NEBULAFORGE_SKILLS_PATH;
    else process.env.NEBULAFORGE_SKILLS_PATH = original;
  });

  it('rejects traversal and oversized requests', async () => {
    const registry = new SkillRegistry();
    await expect(registry.get(['../secrets'])).rejects.toThrow('Invalid skill name');
    await expect(registry.get(['a', 'b', 'c', 'd', 'e', 'f'])).rejects.toThrow('between 1 and 5');
  });
});
