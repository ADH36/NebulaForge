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

  async generateAgentConfig(agent: unknown, importMode = false): Promise<{ file: string; created: boolean }> {
    const normalized = typeof agent === 'string' ? agent.trim().toLowerCase() : 'codex';
    const fileByAgent: Record<string, string> = {
      codex: 'AGENTS.md', cursor: 'AGENTS.md', hermes: 'AGENTS.md',
      claude: 'CLAUDE.md', claude_code: 'CLAUDE.md',
      gemini: 'GEMINI.md', gemini_cli: 'GEMINI.md',
      copilot: path.join('.github', 'copilot-instructions.md')
    };
    const file = fileByAgent[normalized];
    if (!file) throw new Error('agent must be codex, cursor, claude, gemini, or copilot');
    const target = path.resolve(process.cwd(), file);
    const root = path.resolve(process.cwd());
    if (!target.startsWith(root + path.sep)) throw new Error('agent configuration path escaped the project root');
    const markerStart = '<!-- NEBULAFORGE_SKILLS_START -->';
    const markerEnd = '<!-- NEBULAFORGE_SKILLS_END -->';
    const body = importMode
      ? `${markerStart}\n@import docs/skills/README.md\n${markerEnd}`
      : `${markerStart}\n## NebulaForge Unreal workflow guidance\n\nUse the runtime \`list_skills\` tool before complex Unreal edits, then load only the relevant packs with \`get_skills\`. Batch related operations with \`execute_python_code\` or the consolidated domain tools. Discover Python APIs with \`discover_python_class\` before calling unfamiliar \`unreal.*\` services. Compile, save, and request verification evidence after mutations.\n${markerEnd}`;
    let existing = '';
    try { existing = await fs.readFile(target, 'utf8'); } catch { existing = ''; }
    const blockPattern = new RegExp(`${markerStart}[\\s\\S]*?${markerEnd}`, 'm');
    const updated = blockPattern.test(existing) ? existing.replace(blockPattern, body) : `${existing}${existing && !existing.endsWith('\n') ? '\n' : ''}\n${body}\n`;
    await fs.mkdir(path.dirname(target), { recursive: true });
    await fs.writeFile(target, updated, 'utf8');
    return { file, created: existing.length === 0 };
  }
}
