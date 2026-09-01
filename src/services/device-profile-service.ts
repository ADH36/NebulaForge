import { readProjectFile, writeProjectFile } from './project-file-service.js';

const PROFILE_NAME = /^[A-Za-z][A-Za-z0-9_]{0,127}$/;
const PROFILE_TYPE = /^[A-Za-z][A-Za-z0-9_]{0,63}$/;
const CVAR_NAME = /^[A-Za-z][A-Za-z0-9_.-]{0,127}$/;

export async function createDeviceProfile(options: {
  projectPath?: string;
  profileName: string;
  profileType: string;
  parentProfileName?: string;
  cvars?: Record<string, string | number | boolean>;
  backup?: boolean;
}): Promise<Record<string, unknown>> {
  const name = options.profileName.trim();
  const type = options.profileType.trim();
  const parent = options.parentProfileName?.trim() || '';
  if (!PROFILE_NAME.test(name) || !PROFILE_TYPE.test(type) || (parent && !PROFILE_NAME.test(parent))) {
    return { success: false, error: 'INVALID_ARGUMENT', message: 'profileName, profileType, and parentProfileName must be safe Unreal profile identifiers' };
  }
  const cvars = Object.entries(options.cvars ?? {});
  if (cvars.length > 64 || cvars.some(([key, value]) => !CVAR_NAME.test(key) || /[\r\n]/.test(String(value)))) {
    return { success: false, error: 'INVALID_ARGUMENT', message: 'cvars must contain at most 64 safe names and scalar values' };
  }
  const existing = await readProjectFile(options.projectPath, 'Config/DefaultDeviceProfiles.ini');
  const existingContent = existing.success === true ? String(existing.content ?? '') : '';
  const section = `[${name} DeviceProfile]`;
  if (existingContent.split(/\r?\n/).some(line => line.trim() === section)) {
    return { success: false, error: 'PROFILE_EXISTS', message: `Device profile already exists: ${name}`, profileName: name };
  }
  const lines = [section, `DeviceType=${type}`];
  if (parent) lines.push(`BaseProfileName=${parent}`);
  for (const [key, value] of cvars) lines.push(`+CVars=${key}=${String(value)}`);
  const prefix = existingContent.length > 0 && !existingContent.endsWith('\n') ? '\n' : '';
  const written = await writeProjectFile(options.projectPath, 'Config/DefaultDeviceProfiles.ini', `${existingContent}${prefix}${lines.join('\n')}\n`, options.backup !== false);
  return { ...written, profileName: name, profileType: type, parentProfileName: parent || undefined, cvarCount: cvars.length, configName: 'DefaultDeviceProfiles.ini' };
}
