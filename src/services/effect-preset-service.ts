import { readProjectFile, writeProjectFile } from './project-file-service.js';

const MAX_ACTIONS = 128;
const MAX_ACTION_ARGUMENTS = 256 * 1024;
const NAME_PATTERN = /^[A-Za-z0-9][A-Za-z0-9_.-]{0,127}$/;

export interface EffectPresetAction {
  action: string;
  args: Record<string, unknown>;
}

export interface EffectPreset {
  version: 1;
  name: string;
  actions: EffectPresetAction[];
}

function safeName(value: unknown): string {
  if (typeof value !== 'string' || !NAME_PATTERN.test(value.trim())) throw new Error('name must be a safe effect preset identifier.');
  return value.trim();
}

function safePresetPath(value: unknown): string {
  if (typeof value !== 'string' || !value.trim().toLowerCase().endsWith('.json')) throw new Error('presetPath must be a project-relative .json path.');
  const normalized = value.trim().replace(/\\/g, '/');
  if (!normalized.startsWith('Config/Effects/') && !normalized.startsWith('Content/Effects/')) throw new Error('presetPath must be under Config/Effects or Content/Effects.');
  return normalized;
}

function normalizeAction(value: unknown): EffectPresetAction {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Each effect preset action must be an object.');
  const record = value as Record<string, unknown>;
  if (typeof record.action !== 'string' || record.action.trim().length === 0 || record.action.length > 128) throw new Error('Each effect preset action requires a bounded action name.');
  if (!record.args || typeof record.args !== 'object' || Array.isArray(record.args)) throw new Error('Each effect preset action requires an args object.');
  const args = record.args as Record<string, unknown>;
  if (Buffer.byteLength(JSON.stringify(args), 'utf8') > MAX_ACTION_ARGUMENTS) throw new Error(`Each action args object must be at most ${MAX_ACTION_ARGUMENTS} bytes.`);
  return { action: record.action.trim(), args };
}

export function validateEffectPreset(value: unknown): EffectPreset {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Effect preset must be a JSON object.');
  const record = value as Record<string, unknown>;
  if (record.version !== 1) throw new Error('Effect preset version must be 1.');
  if (!Array.isArray(record.actions) || record.actions.length === 0 || record.actions.length > MAX_ACTIONS) throw new Error(`actions must contain between 1 and ${MAX_ACTIONS} items.`);
  const actions = record.actions.map(normalizeAction);
  if (actions.some(entry => ['create_effect_preset', 'apply_effect_preset', 'validate_effect_preset'].includes(entry.action))) throw new Error('Effect presets cannot invoke preset-management actions recursively.');
  return { version: 1, name: safeName(record.name), actions };
}

async function readPreset(projectPath: string | undefined, presetPath: string): Promise<EffectPreset> {
  const file = await readProjectFile(projectPath, presetPath);
  return validateEffectPreset(JSON.parse(String(file.content)));
}

export async function createEffectPreset(input: { projectPath?: string; presetPath: string; name: string; actions: unknown[]; backup?: boolean }): Promise<Record<string, unknown>> {
  const path = safePresetPath(input.presetPath);
  const preset = validateEffectPreset({ version: 1, name: input.name, actions: input.actions });
  const result = await writeProjectFile(input.projectPath, path, `${JSON.stringify(preset, null, 2)}\n`, input.backup !== false);
  return { ...result, name: preset.name, actionCount: preset.actions.length };
}

export async function loadEffectPreset(input: { projectPath?: string; presetPath: string }): Promise<EffectPreset> {
  return readPreset(input.projectPath, safePresetPath(input.presetPath));
}
