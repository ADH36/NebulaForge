import fs from 'node:fs/promises';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { jobManager } from './job-manager.js';

export interface ProjectValidationOptions {
  projectPath?: string;
  requiredFiles?: string[];
  requiredDirectories?: string[];
  includeInventory?: boolean;
  validationMode?: 'static' | 'data_validation';
  enginePath?: string;
  validationArguments?: string[];
  timeoutMs?: number;
}

export interface UnrealAutomationTestOptions {
  projectPath?: string;
  enginePath?: string;
  filter?: string;
  test?: string;
  timeoutMs?: number;
  reportPath?: string;
}

export interface ProjectValidationCheck {
  name: string;
  success: boolean;
  message: string;
  path?: string;
}

const MAX_REQUIRED_ENTRIES = 128;
const MAX_INVENTORY_ENTRIES = 20000;
const IGNORED_DIRECTORIES = new Set(['Binaries', 'DerivedDataCache', 'Intermediate', 'Saved']);
const MAX_VALIDATION_ARGUMENTS = 64;
const MAX_VALIDATION_ARGUMENT_LENGTH = 512;

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

interface ProjectInventory {
  files: number;
  directories: number;
  assets: number;
  maps: number;
  configs: number;
  truncated: boolean;
}

async function collectInventory(root: string): Promise<ProjectInventory> {
  const inventory: ProjectInventory = { files: 0, directories: 0, assets: 0, maps: 0, configs: 0, truncated: false };
  const pending = [root];
  while (pending.length > 0 && inventory.files + inventory.directories < MAX_INVENTORY_ENTRIES) {
    const current = pending.pop();
    if (!current) break;
    let entries;
    try {
      entries = await fs.readdir(current, { withFileTypes: true });
    } catch {
      continue;
    }
    for (const entry of entries) {
      if (inventory.files + inventory.directories >= MAX_INVENTORY_ENTRIES) {
        inventory.truncated = true;
        break;
      }
      if (entry.isDirectory()) {
        if (!IGNORED_DIRECTORIES.has(entry.name)) {
          inventory.directories += 1;
          pending.push(path.join(current, entry.name));
        }
        continue;
      }
      if (!entry.isFile()) continue;
      inventory.files += 1;
      const extension = path.extname(entry.name).toLowerCase();
      if (extension === '.uasset' || extension === '.umap') inventory.assets += 1;
      if (extension === '.umap') inventory.maps += 1;
      if (extension === '.ini') inventory.configs += 1;
    }
  }
  return inventory;
}

function resolveEngineRoot(enginePath?: string): string | undefined {
  const configured = enginePath || process.env.UE_ENGINE_PATH || process.env.UNREAL_ENGINE_PATH;
  if (!configured) return undefined;
  const absolute = path.resolve(configured);
  return path.basename(absolute).toLowerCase() === 'engine' ? absolute : path.join(absolute, 'Engine');
}

async function findEditorCommandlet(enginePath?: string): Promise<string | undefined> {
  const engineRoot = resolveEngineRoot(enginePath);
  if (!engineRoot) return undefined;
  const candidates = process.platform === 'win32'
    ? [path.join(engineRoot, 'Binaries', 'Win64', 'UnrealEditor-Cmd.exe')]
    : process.platform === 'darwin'
      ? [path.join(engineRoot, 'Binaries', 'Mac', 'UnrealEditor-Cmd')]
      : [path.join(engineRoot, 'Binaries', 'Linux', 'UnrealEditor-Cmd')];
  for (const candidate of candidates) {
    try {
      const stats = await fs.stat(candidate);
      if (stats.isFile()) return candidate;
    } catch { /* continue probing */ }
  }
  return undefined;
}

async function resolveProjectDescriptor(root: string, projectPath?: string): Promise<string | undefined> {
  if (projectPath && projectPath.toLowerCase().endsWith('.uproject')) return path.resolve(projectPath);
  try {
    const entries = await fs.readdir(root, { withFileTypes: true });
    const descriptors = entries.filter((entry) => entry.isFile() && entry.name.toLowerCase().endsWith('.uproject'));
    return descriptors.length === 1 ? path.join(root, descriptors[0].name) : undefined;
  } catch {
    return undefined;
  }
}

function safeAutomationFilter(value: unknown): string {
  const filter = typeof value === 'string' ? value.trim() : '';
  if (filter.length > 256 || (filter && !/^[A-Za-z0-9_.*?/:,-]+$/.test(filter))) {
    throw new Error('test filter contains unsupported characters or is too long');
  }
  return filter;
}

function resolveAutomationReport(root: string, reportPath: unknown): { absolute: string; relative: string } | undefined {
  if (typeof reportPath !== 'string' || reportPath.trim() === '') return undefined;
  const relative = path.normalize(reportPath.trim());
  if (path.isAbsolute(relative) || relative === '..' || relative.startsWith(`..${path.sep}`) || relative.includes(':')) return undefined;
  const filename = path.basename(relative);
  if (!/^[A-Za-z0-9._-]{1,128}\.json$/i.test(filename)) return undefined;
  return { absolute: path.join(root, 'Saved', 'AutomationReports', filename), relative: path.join('Saved', 'AutomationReports', filename) };
}

export function summarizeAutomationOutput(output: string, errorOutput: string): Record<string, unknown> {
  const counts = { passed: 0, failed: 0, skipped: 0, detected: 0 };
  const statuses = new Set(['passed', 'succeeded', 'success', 'failed', 'error', 'skipped', 'notrun']);
  for (const line of `${output}\n${errorOutput}`.split(/\r?\n/)) {
    const match = line.match(/\b(Passed|Succeeded|Success|Failed|Error|Skipped|NotRun)\b/i);
    if (!match || !statuses.has(match[1].toLowerCase())) continue;
    const status = match[1].toLowerCase();
    counts.detected += 1;
    if (status === 'passed' || status === 'succeeded' || status === 'success') counts.passed += 1;
    else if (status === 'failed' || status === 'error') counts.failed += 1;
    else counts.skipped += 1;
  }
  return { ...counts, source: 'output_heuristic', authoritative: false };
}

export async function runUnrealAutomationTests(options: UnrealAutomationTestOptions = {}): Promise<Record<string, unknown>> {
  const root = resolveRoot(options.projectPath);
  if (!root) return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'projectPath or UE_PROJECT_PATH is required' };
  const projectFile = await resolveProjectDescriptor(root, options.projectPath);
  if (!projectFile) return { success: false, error: 'PROJECT_DESCRIPTOR_INVALID', message: 'Unable to identify a unique .uproject descriptor' };
  let filter: string;
  try {
    filter = safeAutomationFilter(options.test || options.filter);
  } catch (error) {
    return { success: false, error: 'INVALID_ARGUMENT', message: String(error) };
  }
  const automationCommand = filter ? `Automation RunTests ${filter}` : 'Automation RunAll';
  const report = resolveAutomationReport(root, options.reportPath);
  if (options.reportPath !== undefined && !report) {
    return { success: false, error: 'INVALID_REPORT_PATH', message: 'reportPath must be a relative JSON filename stored under Saved/AutomationReports' };
  }
  const executable = await findEditorCommandlet(options.enginePath);
  if (!executable) {
    return {
      success: false,
      error: 'UNREAL_EDITOR_CMD_NOT_FOUND',
      message: 'UnrealEditor-Cmd was not found. Set enginePath or UE_ENGINE_PATH to a valid Unreal Engine root.'
    };
  }
  const commandletArgs = [
    projectFile,
    '-unattended',
    '-nop4',
    '-nosplash',
    '-nosound',
    '-NullRHI',
    `-ExecCmds=${automationCommand}`,
    '-TestExit=Automation Test Queue Empty'
  ];
  const child = spawn(executable, commandletArgs, { shell: false, stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true });
  const job = jobManager.startProcess({
    label: `run_tests:${filter || 'all'}/${path.basename(projectFile)}`,
    process: child,
    timeoutMs: options.timeoutMs,
    onComplete: report ? async (completedJob) => {
      await fs.mkdir(path.dirname(report.absolute), { recursive: true });
      await fs.writeFile(report.absolute, `${JSON.stringify({
        schemaVersion: 1,
        jobId: completedJob.jobId,
        label: completedJob.label,
        status: completedJob.status,
        startedAt: completedJob.startedAt,
        finishedAt: completedJob.finishedAt,
        exitCode: completedJob.exitCode,
        signal: completedJob.signal,
        projectFile,
        filter,
        command: automationCommand,
        testSummary: summarizeAutomationOutput(completedJob.output, completedJob.errorOutput),
        output: completedJob.output,
        errorOutput: completedJob.errorOutput,
        outputTruncated: completedJob.outputTruncated
      }, null, 2)}\n`, 'utf8');
    } : undefined
  });
  return {
    success: true,
    started: true,
    jobId: job.jobId,
    status: job.status,
    projectFile,
    filter,
    command: automationCommand,
    ...(report ? { reportPath: report.relative } : {}),
    message: 'Unreal automation test job started; poll system_control.get_job_status for terminal exit and output.'
  };
}

function validateCommandletArguments(values: string[] | undefined): string[] | undefined {
  if (!values) return [];
  if (values.length > MAX_VALIDATION_ARGUMENTS) throw new Error(`validationArguments cannot contain more than ${MAX_VALIDATION_ARGUMENTS} entries`);
  const normalized = values.map((value) => typeof value === 'string' ? value.trim() : '');
  if (normalized.some((value) => !value || value.length > MAX_VALIDATION_ARGUMENT_LENGTH || !/^[A-Za-z0-9_./:=+\\-]+$/.test(value))) {
    throw new Error('validationArguments contains an invalid commandlet token');
  }
  if (normalized.some((value) => /^-?(run|project|game|engine|execcmds)=/i.test(value))) {
    throw new Error('validationArguments cannot override the managed project or commandlet');
  }
  return normalized;
}

function moduleName(value: unknown): string | undefined {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return undefined;
  const name = (value as Record<string, unknown>).Name;
  return typeof name === 'string' && /^[A-Za-z][A-Za-z0-9_]*$/.test(name) ? name : undefined;
}

async function validateProjectModules(root: string, modules: unknown[]): Promise<ProjectValidationCheck> {
  const names = modules.map(moduleName).filter((name): name is string => Boolean(name));
  const malformed = modules.length !== names.length;
  const missing: string[] = [];
  for (const name of names) {
    const moduleRoot = path.join(root, 'Source', name);
    if (!(await existsAs(root, path.join('Source', name), true))) {
      missing.push(name);
      continue;
    }
    const files = await fs.readdir(moduleRoot).catch(() => []);
    if (!files.some((file) => file.toLowerCase() === `${name.toLowerCase()}.build.cs` || file.toLowerCase() === `${name.toLowerCase()}.target.cs`)) {
      missing.push(name);
    }
  }
  return {
    name: 'project_module_layout',
    success: !malformed && missing.length === 0,
    message: malformed ? 'One or more module entries are malformed' : missing.length > 0 ? `Missing Source module layout: ${missing.join(', ')}` : `Validated ${names.length} project module(s)`,
  };
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
    if (Array.isArray(modules)) checks.push(await validateProjectModules(root, modules));

    if (Array.isArray(plugins)) {
      const projectPlugins = plugins.map((entry) => entry && typeof entry === 'object' && !Array.isArray(entry) ? entry as Record<string, unknown> : undefined);
      const malformedPluginNames = projectPlugins.filter((entry) => entry && (typeof entry.Name !== 'string' || !/^[A-Za-z][A-Za-z0-9_]*$/.test(entry.Name))).length;
      checks.push({
        name: 'project_plugin_identifiers',
        success: malformedPluginNames === 0,
        message: malformedPluginNames === 0 ? `Validated ${plugins.length} declared plugin identifier(s)` : `${malformedPluginNames} declared plugin identifier(s) are malformed`
      });
    }
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

  let inventory: ProjectInventory | undefined;
  if (options.includeInventory !== false) {
    inventory = await collectInventory(root);
    checks.push({
      name: 'project_content_inventory',
      success: inventory.assets >= inventory.maps,
      message: `Inventory found ${inventory.assets} Unreal asset(s), ${inventory.maps} map(s), and ${inventory.configs} config file(s)${inventory.truncated ? ' (bounded scan)' : ''}`
    });
  }

  const staticSuccess = checks.length > 0 && checks.every((check) => check.success);
  const validationMode = options.validationMode ?? 'static';
  if (validationMode !== 'static' && validationMode !== 'data_validation') {
    return { success: false, root, projectFile, checks, error: 'INVALID_VALIDATION_MODE', message: 'validationMode must be static or data_validation' };
  }

  if (validationMode === 'data_validation') {
    if (!staticSuccess) {
      return {
        success: false,
        root,
        projectFile,
        checks,
        failedChecks: checks.filter((check) => !check.success).map((check) => check.name),
        inventory,
        validation: { mode: validationMode, started: false, reason: 'static_validation_failed' },
        message: 'Live Data Validation was not started because static project validation failed'
      };
    }
    let validationArguments: string[] | undefined;
    try {
      validationArguments = validateCommandletArguments(options.validationArguments);
    } catch (error) {
      return { success: false, root, projectFile, checks, inventory, error: 'INVALID_ARGUMENT', message: String(error) };
    }
    const executable = await findEditorCommandlet(options.enginePath);
    if (!executable) {
      return {
        success: false,
        root,
        projectFile,
        checks,
        inventory,
        validation: { mode: validationMode, started: false },
        error: 'UNREAL_EDITOR_CMD_NOT_FOUND',
        message: 'UnrealEditor-Cmd was not found. Set enginePath or UE_ENGINE_PATH to a valid Unreal Engine root.'
      };
    }
    const projectFilePath = path.join(root, projectFile);
    const commandletArgs = [projectFilePath, '-run=DataValidation', '-unattended', '-nop4', '-nosplash', '-nosound', '-NullRHI', ...(validationArguments ?? [])];
    const child = spawn(executable, commandletArgs, { shell: false, stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true });
    const job = jobManager.startProcess({
      label: `validate_project:data_validation/${projectFile}`,
      process: child,
      timeoutMs: options.timeoutMs
    });
    return {
      success: true,
      root,
      projectFile,
      checks,
      failedChecks: [],
      inventory,
      validation: {
        mode: validationMode,
        started: true,
        jobId: job.jobId,
        status: job.status,
        commandlet: 'DataValidation'
      },
      message: 'Unreal Data Validation commandlet started; poll the returned jobId with system_control.get_job_status'
    };
  }

  return {
    success: staticSuccess,
    root,
    projectFile,
    checks,
    failedChecks: checks.filter((check) => !check.success).map((check) => check.name),
    inventory,
    message: staticSuccess ? 'Project validation passed' : 'Project validation found blocking issues'
  };
}
