import { readProjectFile, writeProjectFile } from './project-file-service.js';

const TAG_PATTERN = /^[A-Za-z][A-Za-z0-9_.-]{0,127}$/;
const SECTION = '[/Script/GameplayTags.GameplayTagsSettings]';
const TAG_LINE = /^\+GameplayTagList=\(Tag="([^"]+)",DevComment="([^"]*)"\)$/;

function validateTag(tag: string): void {
  if (!TAG_PATTERN.test(tag)) throw new Error('tag must contain only letters, numbers, dots, underscores, or hyphens and start with a letter.');
}

async function loadConfig(projectPath?: string): Promise<{ content: string; exists: boolean }> {
  try {
    const result = await readProjectFile(projectPath, 'Config/DefaultGameplayTags.ini');
    return { content: String(result.content), exists: true };
  } catch {
    return { content: '', exists: false };
  }
}

function parseTags(content: string): Array<{ tag: string; comment: string }> {
  return content.split(/\r?\n/).flatMap(line => {
    const match = line.trim().match(TAG_LINE);
    return match ? [{ tag: match[1], comment: match[2] }] : [];
  });
}

export async function listGameplayTags(projectPath?: string): Promise<Record<string, unknown>> {
  const config = await loadConfig(projectPath);
  return { success: true, configPath: 'Config/DefaultGameplayTags.ini', tags: parseTags(config.content), count: parseTags(config.content).length, exists: config.exists };
}

export async function addGameplayTag(projectPath: string | undefined, tag: string, comment: string = '', backup: boolean = true): Promise<Record<string, unknown>> {
  validateTag(tag);
  if (comment.includes('"') || comment.includes('\r') || comment.includes('\n')) throw new Error('comment cannot contain quotes or newlines.');
  const config = await loadConfig(projectPath);
  const existing = parseTags(config.content);
  if (existing.some(entry => entry.tag === tag)) return { success: true, changed: false, tag, message: 'Gameplay Tag already exists.' };
  let content = config.content.trimEnd();
  if (!content.includes(SECTION)) content = `${content}${content ? '\n\n' : ''}${SECTION}`;
  content += `\n+GameplayTagList=(Tag="${tag}",DevComment="${comment}")\n`;
  const write = await writeProjectFile(projectPath, 'Config/DefaultGameplayTags.ini', content, backup);
  return { success: true, changed: true, tag, write };
}

export async function removeGameplayTag(projectPath: string | undefined, tag: string, backup: boolean = true): Promise<Record<string, unknown>> {
  validateTag(tag);
  const config = await loadConfig(projectPath);
  const lines = config.content.split(/\r?\n/);
  const filtered = lines.filter(line => {
    const match = line.trim().match(TAG_LINE);
    return !match || match[1] !== tag;
  });
  if (filtered.length === lines.length) return { success: true, changed: false, tag, message: 'Gameplay Tag did not exist.' };
  const write = await writeProjectFile(projectPath, 'Config/DefaultGameplayTags.ini', filtered.join('\n'), backup);
  return { success: true, changed: true, tag, write };
}
