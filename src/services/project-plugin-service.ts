import fs from 'node:fs/promises';
import path from 'node:path';
import { writeProjectFile } from './project-file-service.js';

const PLUGIN_NAME = /^[A-Za-z][A-Za-z0-9_]{0,127}$/;

function projectRoot(projectPath?: string): string | undefined {
  const configured = projectPath || process.env.UE_PROJECT_PATH;
  if (!configured) return undefined;
  const absolute = path.resolve(configured);
  return absolute.toLowerCase().endsWith('.uproject') ? path.dirname(absolute) : absolute;
}

async function findDescriptor(root: string, projectPath?: string): Promise<string | undefined> {
  const configured = projectPath || process.env.UE_PROJECT_PATH;
  if (configured && configured.toLowerCase().endsWith('.uproject')) return path.basename(configured);
  const entries = await fs.readdir(root, { withFileTypes: true });
  const candidates = entries.filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith('.uproject')).map((entry) => entry.name);
  return candidates.length === 1 ? candidates[0] : undefined;
}

export async function manageProjectPlugins(projectPath: string | undefined, pluginAction: string, pluginName?: string, backup = true): Promise<Record<string, unknown>> {
  const root = projectRoot(projectPath);
  if (!root) return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'projectPath or UE_PROJECT_PATH is required' };
  const descriptorName = await findDescriptor(root, projectPath);
  if (!descriptorName) return { success: false, error: 'PROJECT_DESCRIPTOR_INVALID', message: 'Expected exactly one .uproject descriptor' };

  const descriptorPath = path.join(root, descriptorName);
  let descriptor: Record<string, unknown>;
  try {
    descriptor = JSON.parse(await fs.readFile(descriptorPath, 'utf8')) as Record<string, unknown>;
  } catch (error) {
    return { success: false, error: 'PROJECT_DESCRIPTOR_INVALID', message: `Unable to parse ${descriptorName}: ${String(error)}` };
  }

  const rawPlugins = Array.isArray(descriptor.Plugins) ? descriptor.Plugins : [];
  const plugins = rawPlugins.filter((entry): entry is Record<string, unknown> => Boolean(entry && typeof entry === 'object' && !Array.isArray(entry)));
  if (pluginAction === 'validate') {
    const localDescriptors = new Map<string, Record<string, unknown>>();
    const pluginsRoot = path.join(root, 'Plugins');
    try {
      const pluginFolders = await fs.readdir(pluginsRoot, { withFileTypes: true });
      for (const folder of pluginFolders.filter((entry) => entry.isDirectory())) {
        const descriptorPath = path.join(pluginsRoot, folder.name, `${folder.name}.uplugin`);
        try {
          localDescriptors.set(folder.name, JSON.parse(await fs.readFile(descriptorPath, 'utf8')) as Record<string, unknown>);
        } catch {
          // Missing or malformed local descriptors are reported when declared.
        }
      }
    } catch {
      // A project may not have a local Plugins directory.
    }
    const missingLocalDescriptors = plugins
      .map((entry) => typeof entry.Name === 'string' ? entry.Name : undefined)
      .filter((name): name is string => typeof name === 'string' && !localDescriptors.has(name));
    const dependencyIssues: Array<Record<string, string>> = [];
    for (const [name, pluginDescriptor] of localDescriptors) {
      const dependencies = Array.isArray(pluginDescriptor.Plugins) ? pluginDescriptor.Plugins : [];
      for (const dependency of dependencies) {
        if (!dependency || typeof dependency !== 'object' || Array.isArray(dependency)) continue;
        const dependencyName = (dependency as Record<string, unknown>).Name;
        if (typeof dependencyName === 'string' && !localDescriptors.has(dependencyName) && !plugins.some((entry) => entry.Name === dependencyName)) {
          dependencyIssues.push({ plugin: name, dependency: dependencyName });
        }
      }
    }
    const valid = missingLocalDescriptors.length === 0 && dependencyIssues.length === 0;
    return {
      success: valid,
      projectFile: descriptorName,
      declaredPlugins: plugins,
      missingLocalDescriptors,
      dependencyIssues,
      message: valid ? 'Project plugin declarations passed local dependency validation' : 'Project plugin dependency validation found issues'
    };
  }
  if (pluginAction === 'list') {
    return { success: true, projectFile: descriptorName, plugins };
  }
  if (pluginAction === 'status') {
    if (!pluginName || !PLUGIN_NAME.test(pluginName)) return { success: false, error: 'INVALID_PLUGIN_NAME', message: 'pluginName must be a valid Unreal plugin identifier' };
    const entry = plugins.find((candidate) => candidate.Name === pluginName);
    if (!entry) return { success: false, error: 'PLUGIN_NOT_DECLARED', message: `Plugin ${pluginName} is not declared in ${descriptorName}` };
    return { success: true, projectFile: descriptorName, pluginName, declared: true, enabled: entry.Enabled !== false, descriptor: entry };
  }
  if (pluginAction !== 'enable' && pluginAction !== 'disable') {
    return { success: false, error: 'INVALID_PLUGIN_ACTION', message: 'pluginAction must be list, status, validate, enable, or disable' };
  }
  if (!pluginName || !PLUGIN_NAME.test(pluginName)) {
    return { success: false, error: 'INVALID_PLUGIN_NAME', message: 'pluginName must be a valid Unreal plugin identifier' };
  }
  const entry = plugins.find((candidate) => candidate.Name === pluginName);
  if (!entry) {
    return { success: false, error: 'PLUGIN_NOT_DECLARED', message: `Plugin ${pluginName} is not declared in ${descriptorName}` };
  }
  const enabled = pluginAction === 'enable';
  entry.Enabled = enabled;
  descriptor.Plugins = plugins;
  const content = `${JSON.stringify(descriptor, null, 2)}\n`;
  const result = await writeProjectFile(projectPath, descriptorName, content, backup);
  return { ...result, projectFile: descriptorName, pluginName, enabled };
}
