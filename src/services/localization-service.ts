import { readProjectFile, writeProjectFile } from './project-file-service.js';

const MAX_ENTRIES = 10000;
const MAX_TEXT_LENGTH = 16384;
const CULTURE_PATTERN = /^[A-Za-z]{2,3}(?:[-_][A-Za-z0-9]{2,8})?$/;
const KEY_PATTERN = /^[A-Za-z0-9][A-Za-z0-9_.-]{0,255}$/;

export interface LocalizationEntry {
  key: string;
  sourceText: string;
  translations?: Record<string, string>;
  voiceAssets?: Record<string, string>;
}

interface LocalizationManifest {
  version: 1;
  targetName: string;
  sourceCulture: string;
  cultures: string[];
  entries: LocalizationEntry[];
}

function requireCulture(value: unknown, field: string): string {
  if (typeof value !== 'string' || !CULTURE_PATTERN.test(value.trim())) throw new Error(`${field} must be a valid culture code.`);
  return value.trim();
}

function requireKey(value: unknown): string {
  if (typeof value !== 'string' || !KEY_PATTERN.test(value.trim())) throw new Error('key must contain only letters, numbers, dot, dash, or underscore.');
  return value.trim();
}

function requireText(value: unknown, field: string): string {
  if (typeof value !== 'string' || value.length === 0 || value.length > MAX_TEXT_LENGTH) throw new Error(`${field} must be a non-empty string of at most ${MAX_TEXT_LENGTH} characters.`);
  return value;
}

function requireManifestPath(value: unknown): string {
  if (typeof value !== 'string' || !value.trim().toLowerCase().endsWith('.json')) throw new Error('manifestPath must be a project-relative .json path.');
  const normalized = value.trim().replace(/\\/g, '/');
  if (!normalized.startsWith('Content/Localization/') && !normalized.startsWith('Config/Localization/')) {
    throw new Error('manifestPath must be under Content/Localization or Config/Localization.');
  }
  return normalized;
}

function normalizeMap(value: unknown, field: string, cultures: Set<string>): Record<string, string> | undefined {
  if (value === undefined) return undefined;
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error(`${field} must be an object keyed by culture.`);
  const result: Record<string, string> = {};
  for (const [culture, text] of Object.entries(value as Record<string, unknown>)) {
    const normalizedCulture = requireCulture(culture, `${field} culture`);
    if (!cultures.has(normalizedCulture)) throw new Error(`${field} contains culture not declared by the manifest: ${normalizedCulture}`);
    result[normalizedCulture] = requireText(text, `${field}.${normalizedCulture}`);
  }
  return result;
}

function normalizeEntry(value: unknown, cultures: Set<string>): LocalizationEntry {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Each localization entry must be an object.');
  const record = value as Record<string, unknown>;
  return {
    key: requireKey(record.key),
    sourceText: requireText(record.sourceText ?? record.source, 'sourceText'),
    translations: normalizeMap(record.translations, 'translations', cultures),
    voiceAssets: normalizeMap(record.voiceAssets, 'voiceAssets', cultures)
  };
}

function validateManifest(value: unknown): LocalizationManifest {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Localization manifest must be a JSON object.');
  const record = value as Record<string, unknown>;
  const targetName = requireKey(record.targetName);
  const sourceCulture = requireCulture(record.sourceCulture, 'sourceCulture');
  if (!Array.isArray(record.cultures) || record.cultures.length === 0 || record.cultures.length > 64) throw new Error('cultures must contain between 1 and 64 culture codes.');
  const cultures = record.cultures.map((culture, index) => requireCulture(culture, `cultures[${index}]`));
  if (new Set(cultures).size !== cultures.length) throw new Error('cultures must not contain duplicates.');
  if (!cultures.includes(sourceCulture)) throw new Error('sourceCulture must be included in cultures.');
  if (!Array.isArray(record.entries) || record.entries.length > MAX_ENTRIES) throw new Error(`entries must contain at most ${MAX_ENTRIES} items.`);
  const cultureSet = new Set(cultures);
  const entries = (record.entries ?? []).map(entry => normalizeEntry(entry, cultureSet));
  if (new Set(entries.map(entry => entry.key)).size !== entries.length) throw new Error('entries must not contain duplicate keys.');
  return { version: 1, targetName, sourceCulture, cultures, entries };
}

async function readManifest(projectPath: string | undefined, manifestPath: string): Promise<LocalizationManifest> {
  const file = await readProjectFile(projectPath, manifestPath);
  return validateManifest(JSON.parse(String(file.content)));
}

export async function createLocalizationManifest(input: {
  projectPath?: string;
  manifestPath: string;
  targetName: string;
  sourceCulture: string;
  cultures: string[];
  entries?: unknown[];
  backup?: boolean;
}): Promise<Record<string, unknown>> {
  const manifest = validateManifest({ ...input, version: 1, entries: input.entries ?? [] });
  const manifestPath = requireManifestPath(input.manifestPath);
  const result = await writeProjectFile(input.projectPath, manifestPath, `${JSON.stringify(manifest, null, 2)}\n`, input.backup !== false);
  return { ...result, targetName: manifest.targetName, sourceCulture: manifest.sourceCulture, cultures: manifest.cultures, entryCount: manifest.entries.length };
}

export async function addLocalizationEntry(input: {
  projectPath?: string;
  manifestPath: string;
  entry: unknown;
  replaceExisting?: boolean;
  backup?: boolean;
}): Promise<Record<string, unknown>> {
  const manifestPath = requireManifestPath(input.manifestPath);
  const manifest = await readManifest(input.projectPath, manifestPath);
  const entry = normalizeEntry(input.entry, new Set(manifest.cultures));
  const existingIndex = manifest.entries.findIndex(candidate => candidate.key === entry.key);
  if (existingIndex >= 0 && input.replaceExisting === false) throw new Error(`Localization key already exists: ${entry.key}`);
  if (existingIndex >= 0) manifest.entries[existingIndex] = entry;
  else {
    if (manifest.entries.length >= MAX_ENTRIES) throw new Error(`Manifest cannot exceed ${MAX_ENTRIES} entries.`);
    manifest.entries.push(entry);
  }
  const result = await writeProjectFile(input.projectPath, manifestPath, `${JSON.stringify(manifest, null, 2)}\n`, input.backup !== false);
  return { ...result, key: entry.key, replaced: existingIndex >= 0, entryCount: manifest.entries.length };
}

export async function validateLocalizationManifest(input: {
  projectPath?: string;
  manifestPath: string;
  requiredCultures?: string[];
  requireTranslations?: boolean;
  requireVoiceAssets?: boolean;
}): Promise<Record<string, unknown>> {
  const manifestPath = requireManifestPath(input.manifestPath);
  const manifest = await readManifest(input.projectPath, manifestPath);
  const requiredCultures = (input.requiredCultures ?? manifest.cultures).map((culture, index) => requireCulture(culture, `requiredCultures[${index}]`));
  const missingTranslations: Array<{ key: string; culture: string }> = [];
  const missingVoiceAssets: Array<{ key: string; culture: string }> = [];
  for (const entry of manifest.entries) {
    for (const culture of requiredCultures) {
      if (culture === manifest.sourceCulture) continue;
      if (input.requireTranslations && !entry.translations?.[culture]) missingTranslations.push({ key: entry.key, culture });
      if (input.requireVoiceAssets && !entry.voiceAssets?.[culture]) missingVoiceAssets.push({ key: entry.key, culture });
    }
  }
  const valid = missingTranslations.length === 0 && missingVoiceAssets.length === 0;
  return { success: valid, valid, manifestPath, targetName: manifest.targetName, entryCount: manifest.entries.length, cultures: manifest.cultures, missingTranslations, missingVoiceAssets };
}
