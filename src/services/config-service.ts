import fs from 'node:fs/promises';
import crypto from 'node:crypto';
import path from 'node:path';
import { readProjectFile, writeProjectFile } from './project-file-service.js';

const CONFIG_NAME = /^[A-Za-z][A-Za-z0-9_-]*\.ini$/;
const SECTION_NAME = /^[^\x5b\x5d\r\n]{1,256}$/;
const KEY_NAME = /^[^=\r\n]{1,256}$/;
const MAX_VALUE_LENGTH = 64 * 1024;
type ConfigLocation = { root: string; target: string; relativePath: string };
type ConfigLocationError = { success: false; error: string; message: string };

function projectRoot(projectPath?: string): string | undefined {
  const configured = projectPath || process.env.UE_PROJECT_PATH;
  if (!configured) return undefined;
  return configured.toLowerCase().endsWith('.uproject') ? path.dirname(configured) : configured;
}

function validateConfigName(configName: string): string | undefined {
  const value = configName.trim();
  return CONFIG_NAME.test(value) && !value.includes('..') ? value : undefined;
}

function configRelativePath(configName: string): string {
  return path.posix.join('Config', configName);
}

function validateSectionKey(section: string, key: string): { section: string; key: string } | undefined {
  const normalizedSection = section.trim();
  const normalizedKey = key.trim();
  if (!SECTION_NAME.test(normalizedSection) || !KEY_NAME.test(normalizedKey)) return undefined;
  return { section: normalizedSection, key: normalizedKey };
}

function parseIni(content: string, wantedSection?: string, wantedKey?: string): string | undefined {
  let currentSection = '';
  for (const rawLine of content.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith(';') || line.startsWith('#')) continue;
    const section = /^\[([^\]]+)\]$/.exec(line);
    if (section) {
      currentSection = section[1].trim();
      continue;
    }
    const separator = line.indexOf('=');
    if (separator < 1 || currentSection !== wantedSection) continue;
    const candidateKey = line.slice(0, separator).trim();
    if (candidateKey === wantedKey) return line.slice(separator + 1).trim();
  }
  return undefined;
}

function parseIniSection(content: string, wantedSection: string): Record<string, string> {
  const values: Record<string, string> = {};
  let currentSection = '';
  for (const rawLine of content.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith(';') || line.startsWith('#')) continue;
    const section = /^\[([^\]]+)\]$/.exec(line);
    if (section) {
      currentSection = section[1].trim();
      continue;
    }
    const separator = line.indexOf('=');
    if (separator < 1 || currentSection !== wantedSection) continue;
    values[line.slice(0, separator).trim()] = line.slice(separator + 1).trim();
  }
  return values;
}

function summarizeIni(content: string): { sections: Array<{ name: string; keys: string[] }>; keyCount: number } {
  const sections: Array<{ name: string; keys: string[] }> = [];
  let current: { name: string; keys: string[] } | undefined;
  for (const rawLine of content.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith(';') || line.startsWith('#')) continue;
    const section = /^\[([^\]]+)\]$/.exec(line);
    if (section) {
      current = { name: section[1].trim(), keys: [] };
      sections.push(current);
      continue;
    }
    const separator = line.indexOf('=');
    if (current && separator > 0) current.keys.push(line.slice(0, separator).trim());
  }
  return { sections, keyCount: sections.reduce((count, section) => count + section.keys.length, 0) };
}

async function configFilePath(projectPath: string | undefined, configName: string, allowMissing: boolean): Promise<ConfigLocation | ConfigLocationError> {
  const validName = validateConfigName(configName);
  const root = projectRoot(projectPath);
  if (!root || !validName) return { success: false, error: 'INVALID_ARGUMENT', message: 'projectPath and a safe configName are required' };
  const resolvedRoot = await fs.realpath(root).catch(() => undefined);
  if (!resolvedRoot) return { success: false, error: 'PROJECT_NOT_FOUND', message: `Project root not found: ${root}` };
  const relativePath = configRelativePath(validName);
  const target = path.resolve(resolvedRoot, relativePath);
  const relative = path.relative(resolvedRoot, target);
  if (relative.startsWith('..') || path.isAbsolute(relative)) return { success: false, error: 'SECURITY_VIOLATION', message: 'Config path escapes the project root' };
  if (!allowMissing) {
    const stat = await fs.stat(target).catch(() => undefined);
    if (!stat?.isFile()) return { success: false, error: 'CONFIG_NOT_FOUND', message: `Config file not found: ${relativePath}` };
  }
  return { root: resolvedRoot, target, relativePath };
}

function setIniValue(content: string, section: string, key: string, value: string): { content: string; changed: boolean } {
  const lines = content.split(/\r?\n/);
  let sectionStart = -1;
  let sectionEnd = lines.length;
  for (let index = 0; index < lines.length; index += 1) {
    const match = /^\s*\[([^\]]+)\]\s*$/.exec(lines[index]);
    if (!match) continue;
    if (sectionStart >= 0) {
      sectionEnd = index;
      break;
    }
    if (match[1].trim() === section) sectionStart = index;
  }

  if (sectionStart < 0) {
    const prefix = content.length > 0 && !content.endsWith('\n') ? '\n' : '';
    return { content: `${content}${prefix}[${section}]\n${key}=${value}\n`, changed: true };
  }

  for (let index = sectionStart + 1; index < sectionEnd; index += 1) {
    const match = /^\s*([^=;#\r\n]+?)\s*=/.exec(lines[index]);
    if (!match || match[1].trim() !== key) continue;
    const next = `${match[1].trim()}=${value}`;
    if (lines[index] === next) return { content, changed: false };
    lines[index] = next;
    return { content: `${lines.join('\n')}`, changed: true };
  }

  lines.splice(sectionEnd, 0, `${key}=${value}`);
  return { content: `${lines.join('\n')}`, changed: true };
}

export async function listConfigLayers(projectPath?: string): Promise<Record<string, unknown>> {
  const root = projectRoot(projectPath);
  if (!root) return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'projectPath or UE_PROJECT_PATH is required' };
  const configDir = path.join(root, 'Config');
  let entries: string[];
  try {
    entries = (await fs.readdir(configDir)).filter(name => /^Default[A-Za-z0-9_-]*\.ini$/i.test(name)).sort();
  } catch {
    return { success: false, error: 'CONFIG_DIRECTORY_NOT_FOUND', message: `Config directory not found: ${configDir}` };
  }
  return { success: true, configDirectory: configDir, layers: entries.map(name => ({ name, relativePath: configRelativePath(name) })), count: entries.length };
}

export async function getConfigValue(projectPath: string | undefined, configName: string, section: string, key: string): Promise<Record<string, unknown>> {
  const validName = validateConfigName(configName);
  const validSectionKey = validateSectionKey(section, key);
  if (!validName || !validSectionKey) return { success: false, error: 'INVALID_ARGUMENT', message: 'configName, section, and key are invalid' };
  const result = await readProjectFile(projectPath, configRelativePath(validName));
  if (result.success !== true) return result;
  const value = parseIni(String(result.content ?? ''), validSectionKey.section, validSectionKey.key);
  return { success: true, configName: validName, section: validSectionKey.section, key: validSectionKey.key, value, found: value !== undefined };
}

export async function getConfigSection(projectPath: string | undefined, configName: string, section: string): Promise<Record<string, unknown>> {
  const validName = validateConfigName(configName);
  const normalizedSection = section.trim();
  if (!validName || !SECTION_NAME.test(normalizedSection)) return { success: false, error: 'INVALID_ARGUMENT', message: 'configName and section are invalid' };
  const result = await readProjectFile(projectPath, configRelativePath(validName));
  if (result.success !== true) return result;
  const values = parseIniSection(String(result.content ?? ''), normalizedSection);
  return { success: true, configName: validName, section: normalizedSection, values, keys: Object.keys(values), keyCount: Object.keys(values).length, found: Object.keys(values).length > 0 };
}

export async function setConfigValue(projectPath: string | undefined, configName: string, section: string, key: string, value: string, backup = true): Promise<Record<string, unknown>> {
  const validName = validateConfigName(configName);
  const validSectionKey = validateSectionKey(section, key);
  if (!validName || !validSectionKey || typeof value !== 'string' || value.length > MAX_VALUE_LENGTH || /[\r\n]/.test(value)) {
    return { success: false, error: 'INVALID_ARGUMENT', message: 'configName, section, key, and value must be safe bounded values' };
  }
  let existingContent = '';
  try {
    const existing = await readProjectFile(projectPath, configRelativePath(validName));
    if (existing.success === true) existingContent = String(existing.content ?? '');
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    if (!/ENOENT|not found|does not exist/i.test(message)) return { success: false, error: 'CONFIG_READ_FAILED', message };
  }
  const updated = setIniValue(existingContent, validSectionKey.section, validSectionKey.key, value);
  if (!updated.changed) return { success: true, changed: false, configName: validName, section: validSectionKey.section, key: validSectionKey.key, value };
  const written = await writeProjectFile(projectPath, configRelativePath(validName), updated.content, backup);
  return { ...written, configName: validName, section: validSectionKey.section, key: validSectionKey.key, value, changed: true };
}

export async function createConfigSection(projectPath: string | undefined, configName: string, section: string, backup = true): Promise<Record<string, unknown>> {
  const validName = validateConfigName(configName);
  const normalizedSection = section.trim();
  if (!validName || !SECTION_NAME.test(normalizedSection)) return { success: false, error: 'INVALID_ARGUMENT', message: 'configName and section are invalid' };
  let existingContent = '';
  try {
    const existing = await readProjectFile(projectPath, configRelativePath(validName));
    if (existing.success === true) existingContent = String(existing.content ?? '');
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    if (!/ENOENT|not found|does not exist/i.test(message)) return { success: false, error: 'CONFIG_READ_FAILED', message };
  }
  const alreadyExists = new RegExp(`^\\s*\\[${normalizedSection.replace(/[.*+?^${}()|[\\]\\\\]/g, '\\$&')}\\]\\s*$`, 'm').test(existingContent);
  if (alreadyExists) return { success: true, configName: validName, section: normalizedSection, created: false };
  const prefix = existingContent.length > 0 && !existingContent.endsWith('\n') ? '\n' : '';
  const written = await writeProjectFile(projectPath, configRelativePath(validName), `${existingContent}${prefix}[${normalizedSection}]\n`, backup);
  return { ...written, configName: validName, section: normalizedSection, created: true };
}

export async function reloadConfig(projectPath: string | undefined, configName: string): Promise<Record<string, unknown>> {
  const location = await configFilePath(projectPath, configName, false);
  if ('success' in location) return location;
  const content = await fs.readFile(location.target, 'utf8');
  const summary = summarizeIni(content);
  return {
    success: true,
    configName,
    filePath: location.relativePath,
    bytes: Buffer.byteLength(content),
    contentHash: crypto.createHash('sha256').update(content, 'utf8').digest('hex'),
    sections: summary.sections,
    sectionCount: summary.sections.length,
    keyCount: summary.keyCount,
    reloaded: true
  };
}

export async function flushConfig(projectPath: string | undefined, configName: string): Promise<Record<string, unknown>> {
  const location = await configFilePath(projectPath, configName, false);
  if ('success' in location) return location;
  const handle = await fs.open(location.target, 'r+');
  try {
    await handle.sync();
    const stat = await handle.stat();
    return { success: true, configName, filePath: location.relativePath, bytes: stat.size, flushed: true };
  } finally {
    await handle.close();
  }
}

export async function getConfigHierarchy(projectPath?: string): Promise<Record<string, unknown>> {
  const root = projectRoot(projectPath);
  if (!root) return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'projectPath or UE_PROJECT_PATH is required' };
  const configDir = path.join(root, 'Config');
  const layers: Array<{ name: string; relativePath: string; scope: string; priority: number }> = [];
  const entries = await fs.readdir(configDir, { withFileTypes: true }).catch(() => undefined);
  if (!entries) return { success: false, error: 'CONFIG_DIRECTORY_NOT_FOUND', message: `Config directory not found: ${configDir}` };
  for (const entry of entries) {
    if (entry.isFile() && /^Default[A-Za-z0-9_-]*\.ini$/i.test(entry.name)) {
      layers.push({ name: entry.name, relativePath: configRelativePath(entry.name), scope: 'project_default', priority: 0 });
      continue;
    }
    if (!entry.isDirectory() || !/^[A-Za-z][A-Za-z0-9_-]*$/.test(entry.name)) continue;
    const platformEntries = await fs.readdir(path.join(configDir, entry.name), { withFileTypes: true }).catch(() => []);
    for (const platformEntry of platformEntries) {
      if (platformEntry.isFile() && /^[A-Za-z][A-Za-z0-9_-]*\.ini$/i.test(platformEntry.name)) {
        layers.push({ name: platformEntry.name, relativePath: path.posix.join('Config', entry.name, platformEntry.name), scope: `platform:${entry.name}`, priority: 1 });
      }
    }
  }
  layers.sort((left, right) => left.priority - right.priority || left.relativePath.localeCompare(right.relativePath));
  return { success: true, configDirectory: configDir, layers, count: layers.length, mergeSemantics: 'project layer inventory; Unreal runtime merge precedence requires live engine verification' };
}
