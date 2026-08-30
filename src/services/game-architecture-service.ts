import fs from 'node:fs/promises';
import path from 'node:path';
import { readProjectFile, writeProjectFile } from './project-file-service.js';

const MAX_REQUIREMENTS = 4096;
const NAME_PATTERN = /^[A-Za-z0-9][A-Za-z0-9_.:-]{0,255}$/;
const PATH_PATTERN = /^(Content|Config|Source|Plugins|Script)\/[A-Za-z0-9_./-]+$/;

export type ArchitectureRequirementKind = 'module' | 'asset' | 'test' | 'file' | 'directory';

export interface ArchitectureRequirement {
  id: string;
  kind: ArchitectureRequirementKind;
  path?: string;
  description?: string;
  required: boolean;
}

interface ArchitectureManifest {
  version: 1;
  projectName: string;
  requirements: ArchitectureRequirement[];
}

function requiredName(value: unknown, field: string): string {
  if (typeof value !== 'string' || !NAME_PATTERN.test(value.trim())) throw new Error(`${field} must be a safe identifier.`);
  return value.trim();
}

function requiredPath(value: unknown): string {
  if (typeof value !== 'string' || !PATH_PATTERN.test(value.trim().replace(/\\/g, '/'))) {
    throw new Error('path must be project-relative and start with Content, Config, Source, Plugins, or Script.');
  }
  return value.trim().replace(/\\/g, '/');
}

function normalizeRequirement(value: unknown): ArchitectureRequirement {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Each architecture requirement must be an object.');
  const record = value as Record<string, unknown>;
  const kind = record.kind;
  if (kind !== 'module' && kind !== 'asset' && kind !== 'test' && kind !== 'file' && kind !== 'directory') {
    throw new Error('requirement kind must be module, asset, test, file, or directory.');
  }
  const requirement: ArchitectureRequirement = {
    id: requiredName(record.id, 'id'),
    kind,
    required: record.required !== false
  };
  if (record.path !== undefined) requirement.path = requiredPath(record.path);
  if (record.description !== undefined) {
    if (typeof record.description !== 'string' || record.description.length > 1024) throw new Error('description must be at most 1024 characters.');
    requirement.description = record.description;
  }
  if ((kind === 'asset' || kind === 'file' || kind === 'directory') && !requirement.path) throw new Error(`${kind} requirements require path.`);
  return requirement;
}

function validateManifest(value: unknown): ArchitectureManifest {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Architecture manifest must be a JSON object.');
  const record = value as Record<string, unknown>;
  if (record.version !== 1) throw new Error('Architecture manifest version must be 1.');
  const projectName = requiredName(record.projectName, 'projectName');
  if (!Array.isArray(record.requirements) || record.requirements.length > MAX_REQUIREMENTS) throw new Error(`requirements must contain at most ${MAX_REQUIREMENTS} items.`);
  const requirements = (record.requirements ?? []).map(normalizeRequirement);
  if (new Set(requirements.map(requirement => requirement.id)).size !== requirements.length) throw new Error('requirements must not contain duplicate ids.');
  return { version: 1, projectName, requirements };
}

function manifestPath(value: unknown): string {
  if (typeof value !== 'string' || !value.trim().endsWith('.json')) throw new Error('manifestPath must be a project-relative .json path.');
  const normalized = value.trim().replace(/\\/g, '/');
  if (!normalized.startsWith('Config/Architecture/') && !normalized.startsWith('Content/Architecture/')) throw new Error('manifestPath must be under Config/Architecture or Content/Architecture.');
  return normalized;
}

async function readManifest(projectPath: string | undefined, path: string): Promise<ArchitectureManifest> {
  const file = await readProjectFile(projectPath, path);
  return validateManifest(JSON.parse(String(file.content)));
}

function projectRoot(projectPath?: string): string {
  const configured = projectPath || process.env.UE_PROJECT_PATH;
  if (!configured) throw new Error('UE_PROJECT_PATH or projectPath is required.');
  return configured.toLowerCase().endsWith('.uproject') ? path.dirname(configured) : configured;
}

export async function createGameArchitectureManifest(input: {
  projectPath?: string;
  manifestPath: string;
  projectName: string;
  requirements?: unknown[];
  backup?: boolean;
}): Promise<Record<string, unknown>> {
  const path = manifestPath(input.manifestPath);
  const manifest = validateManifest({ version: 1, projectName: input.projectName, requirements: input.requirements ?? [] });
  const result = await writeProjectFile(input.projectPath, path, `${JSON.stringify(manifest, null, 2)}\n`, input.backup !== false);
  return { ...result, projectName: manifest.projectName, requirementCount: manifest.requirements.length };
}

export async function addArchitectureRequirement(input: {
  projectPath?: string;
  manifestPath: string;
  requirement: unknown;
  replaceExisting?: boolean;
  backup?: boolean;
}): Promise<Record<string, unknown>> {
  const path = manifestPath(input.manifestPath);
  const manifest = await readManifest(input.projectPath, path);
  const requirement = normalizeRequirement(input.requirement);
  const index = manifest.requirements.findIndex(candidate => candidate.id === requirement.id);
  if (index >= 0 && input.replaceExisting === false) throw new Error(`Architecture requirement already exists: ${requirement.id}`);
  if (index >= 0) manifest.requirements[index] = requirement;
  else manifest.requirements.push(requirement);
  if (manifest.requirements.length > MAX_REQUIREMENTS) throw new Error(`Manifest cannot exceed ${MAX_REQUIREMENTS} requirements.`);
  const result = await writeProjectFile(input.projectPath, path, `${JSON.stringify(manifest, null, 2)}\n`, input.backup !== false);
  return { ...result, id: requirement.id, replaced: index >= 0, requirementCount: manifest.requirements.length };
}

export async function validateGameArchitecture(input: {
  projectPath?: string;
  manifestPath: string;
  includeOptional?: boolean;
}): Promise<Record<string, unknown>> {
  const manifestFilePath = manifestPath(input.manifestPath);
  const manifest = await readManifest(input.projectPath, manifestFilePath);
  const checks = await Promise.all(manifest.requirements.map(async requirement => {
    if (!requirement.path || (requirement.required === false && input.includeOptional !== true)) return { ...requirement, present: true, checked: false };
    const filePath = requirement.path;
    try {
      const absolute = path.resolve(projectRoot(input.projectPath), filePath);
      const root = path.resolve(projectRoot(input.projectPath));
      const relative = path.relative(root, absolute);
      if (relative.startsWith('..') || path.isAbsolute(relative)) throw new Error('requirement path escapes project root.');
      const stat = await fs.stat(absolute);
      if (requirement.kind === 'directory' && !stat.isDirectory()) throw new Error('requirement is not a directory.');
      if (requirement.kind !== 'directory' && !stat.isFile()) throw new Error('requirement is not a file.');
      return { ...requirement, present: true, checked: true };
    } catch {
      return { ...requirement, present: false, checked: true };
    }
  }));
  const missingRequired = checks.filter(check => check.required && check.checked && !check.present).map(check => check.id);
  return { success: missingRequired.length === 0, valid: missingRequired.length === 0, manifestPath: manifestFilePath, projectName: manifest.projectName, requirementCount: checks.length, missingRequired, checks };
}
