import { promises as fs } from 'node:fs';
import path from 'node:path';

type SkillEntry = { name: string; skillPath: string; summary: string };

function candidateRoots(): string[] {
  return [
    process.env.NEBULAFORGE_SKILLS_PATH,
    path.join(process.cwd(), 'docs', 'skills'),
    path.join(process.cwd(), 'Content', 'Skills'),
    path.join(process.cwd(), 'Plugins', 'VibeUE', 'Content', 'Skills'),
    path.join(process.cwd(), 'plugins', 'VibeUE', 'Content', 'Skills')
  ].filter((value): value is string => typeof value === 'string' && value.trim().length > 0);
}

async function readSkill(root: string, name: string): Promise<string | undefined> {
  const resolvedRoot = path.resolve(root);
  const resolvedSkill = path.resolve(resolvedRoot, name, 'SKILL.md');
  if (!resolvedSkill.startsWith(resolvedRoot + path.sep)) return undefined;
  try {
    return await fs.readFile(resolvedSkill, 'utf8');
  } catch {
    return undefined;
  }
}

function summary(markdown: string): string {
  const heading = markdown.match(/^#\s+(.+)$/m)?.[1]?.trim();
  if (heading) return heading;
  return markdown.split(/\r?\n/).find(line => line.trim().length > 0)?.trim().slice(0, 200) ?? '';
}

export class SkillRegistry {
  async list(): Promise<SkillEntry[]> {
    const entries = new Map<string, SkillEntry>();
    for (const root of candidateRoots()) {
      let directories: string[];
      try { directories = await fs.readdir(root); } catch { continue; }
      for (const name of directories) {
        const markdown = await readSkill(root, name);
        if (!markdown || entries.has(name)) continue;
        entries.set(name, { name, skillPath: root + '/' + name, summary: summary(markdown) });
      }
    }
    return [...entries.values()].sort((left, right) => left.name.localeCompare(right.name));
  }

  async get(skillPaths: unknown): Promise<Array<SkillEntry & { content: string }>> {
    if (!Array.isArray(skillPaths) || skillPaths.length === 0 || skillPaths.length > 5) throw new Error('skillPaths must contain between 1 and 5 skill names');
    const result: Array<SkillEntry & { content: string }> = [];
    for (const value of skillPaths) {
      if (typeof value !== 'string' || !/^[A-Za-z0-9][A-Za-z0-9_-]{0,127}$/.test(value)) throw new Error('Invalid skill name');
      let found: (SkillEntry & { content: string }) | undefined;
      for (const root of candidateRoots()) {
        const content = await readSkill(root, value);
        if (content) { found = { name: value, skillPath: root + '/' + value, summary: summary(content), content }; break; }
      }
      if (!found) throw new Error('Skill not found: ' + value);
      result.push(found);
    }
    return result;
  }
}
