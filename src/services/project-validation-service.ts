import fs from 'node:fs/promises';
import path from 'node:path';

export interface ProjectValidationOptions {
  projectPath?: string;
  requiredFiles?: string[];
  requiredDirectories?: string[];
}

export interface ProjectValidationCheck {
  name: string;
  success: boolean;
  message: string;
  path?: string;
}

const MAX_REQUIRED_ENTRIES = 128;

function resolveRoot(projectPath?: string): string | undefined {
  const configured = projectPath || process.env.UE_PROJECT_PATH;
  if (!configured) return undefined;
  const absolute = path.resolve(configured);
  return absolute.toLowerCase().endsWith('.uproject') ? path.dirname(absolute) : absolute;
}

function safeRelativeEntry(value: unknown): string | undefined {
  if (typeof value !== 'string') return undefined;
  const trimmed = value.trim();
  if (!trimmed || path.isAbsolute(trimmed)) return undefined;
  const normalized = path.normalize(trimmed);
  if (normalized === '..' || normalized.startsWith(`..${path.sep}`) || normalized.includes(':')) return undefined;
  return normalized;
}

async function existsAs(root: string, relativeEntry: string, directory: boolean): Promise<boolean> {
  try {
    const stats = await fs.stat(path.join(root, relativeEntry));
    return directory ? stats.isDirectory() : stats.isFile();
  } catch {
    return false;
  }
}

export async function validateProject(options: ProjectValidationOptions = {}): Promise<Record<string, unknown>> {
  const root = resolveRoot(options.projectPath);
  if (!root) {
    return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'projectPath or UE_PROJECT_PATH is required', checks: [] };
  }

  const checks: ProjectValidationCheck[] = [];
  const projectFileFromInput = typeof options.projectPath === 'string' && options.projectPath.toLowerCase().endsWith('.uproject')
    ? path.basename(options.projectPath)
    : undefined;
  let projectFile = projectFileFromInput;

  if (!projectFile) {
    try {
      const entries = await fs.readdir(root, { withFileTypes: true });
      const candidates = entries.filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith('.uproject')).map((entry) => entry.name);
      projectFile = candidates.length === 1 ? candidates[0] : undefined;
      checks.push({
        name: 'project_descriptor_discovery',
        success: candidates.length === 1,
        message: candidates.length === 1
          ? `Found project descriptor ${candidates[0]}`
          : candidates.length === 0 ? 'No .uproject descriptor found' : `Expected one .uproject descriptor, found ${candidates.length}`
      });
    } catch (error) {
      checks.push({ name: 'project_root', success: false, message: `Project root is not readable: ${String(error)}` });
    }
  }

  if (!projectFile) {
    return { success: false, root, checks, error: 'PROJECT_DESCRIPTOR_INVALID', message: 'Unable to identify a unique .uproject descriptor' };
  }

  const descriptorPath = path.join(root, projectFile);
  let descriptor: Record<string, unknown> | undefined;
  try {
    const raw = await fs.readFile(descriptorPath, 'utf8');
    const parsed: unknown = JSON.parse(raw);
    if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) throw new Error('descriptor root must be an object');
    descriptor = parsed as Record<string, unknown>;
    checks.push({ name: 'project_descriptor_json', success: true, message: 'Project descriptor is valid JSON', path: projectFile });
  } catch (error) {
    checks.push({ name: 'project_descriptor_json', success: false, message: `Project descriptor is invalid: ${String(error)}`, path: projectFile });
  }

  if (descriptor) {
    const modules = descriptor.Modules;
    const plugins = descriptor.Plugins;
    checks.push({
      name: 'project_modules',
      success: modules === undefined || (Array.isArray(modules) && modules.every((entry) => Boolean(entry && typeof entry === 'object'))),
      message: modules === undefined ? 'No project modules declared' : Array.isArray(modules) ? `Project declares ${modules.length} module(s)` : 'Modules must be an array'
    });
    checks.push({
      name: 'project_plugins',
      success: plugins === undefined || (Array.isArray(plugins) && plugins.every((entry) => Boolean(entry && typeof entry === 'object'))),
      message: plugins === undefined ? 'No project plugins declared' : Array.isArray(plugins) ? `Project declares ${plugins.length} plugin(s)` : 'Plugins must be an array'
    });
  }

  const requiredDirectories = options.requiredDirectories ?? ['Content', 'Config'];
  const directories = requiredDirectories.slice(0, MAX_REQUIRED_ENTRIES);
  for (const rawEntry of directories) {
    const entry = safeRelativeEntry(rawEntry);
    checks.push(entry
      ? { name: 'required_directory', success: await existsAs(root, entry, true), message: `Required directory ${entry}`, path: entry }
      : { name: 'required_directory', success: false, message: 'Required directory must be a safe relative path' });
  }

  const requiredFiles = (options.requiredFiles ?? []).slice(0, MAX_REQUIRED_ENTRIES);
  for (const rawEntry of requiredFiles) {
    const entry = safeRelativeEntry(rawEntry);
    checks.push(entry
      ? { name: 'required_file', success: await existsAs(root, entry, false), message: `Required file ${entry}`, path: entry }
      : { name: 'required_file', success: false, message: 'Required file must be a safe relative path' });
  }

  const success = checks.length > 0 && checks.every((check) => check.success);
  return {
    success,
    root,
    projectFile,
    checks,
    failedChecks: checks.filter((check) => !check.success).map((check) => check.name),
    message: success ? 'Project validation passed' : 'Project validation found blocking issues'
  };
}
