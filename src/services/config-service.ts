import fs from 'node:fs/promises';
import path from 'node:path';
import { readProjectFile, writeProjectFile } from './project-file-service.js';

const CONFIG_NAME = /^[A-Za-z][A-Za-z0-9_-]*\.ini$/;
const SECTION_NAME = /^[^\x5b\x5d\r\n]{1,256}$/;
const KEY_NAME = /^[^=\r\n]{1,256}$/;
const MAX_VALUE_LENGTH = 64 * 1024;

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
