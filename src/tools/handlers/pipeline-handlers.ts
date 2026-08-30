import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { PipelineArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { spawn, exec } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import util from 'node:util';
import { jobManager } from '../../services/job-manager.js';

/** Promisified child_process.exec for async shell commands. */
const execAsync = util.promisify(exec);
const ALLOWED_UBT_PLATFORMS = new Set(['Win64', 'Mac', 'Linux', 'LinuxArm64', 'Android', 'IOS', 'TVOS', 'HoloLens', 'VisionOS']);
const ALLOWED_UBT_CONFIGURATIONS = new Set(['Debug', 'DebugGame', 'Development', 'Shipping', 'Test']);
const BLOCKED_UBT_OVERRIDE_OPTIONS = new Set(['project', 'projectfile', 'target', 'mode']);
const ALLOWED_UAT_OPERATIONS = new Set(['build', 'cook', 'stage', 'package', 'archive', 'build_cook_stage_package', 'build_server', 'package_server', 'archive_server']);
const SIGNING_PASSWORD_ENV = /^[A-Za-z_][A-Za-z0-9_]{0,127}$/;

/** Reject UBT argument strings containing shell-dangerous characters. */
function validateUbtArgumentsString(extraArgs: string): void {
  if (!extraArgs || typeof extraArgs !== 'string') {
    return;
  }

  const forbiddenChars = ['\n', '\r', ';', '|', '`', '&&', '||', '>', '<', '"', "'"];
  for (const char of forbiddenChars) {
    if (extraArgs.includes(char)) {
      throw new Error(
        `UBT arguments contain forbidden character(s) and are blocked for safety. Blocked: ${JSON.stringify(char)}.`
      );
    }
  }

  for (const token of tokenizeArgs(extraArgs)) {
    validateUbtExtraArgumentToken(token);
  }
}

function validateUbtArgumentToken(token: string, context: string): void {
  if (!token || token.trim().length === 0) {
    throw new Error(`${context} must be a non-empty UBT token.`);
  }

  const trimmed = token.trim();
  if (!/^[A-Za-z0-9_\-.=:/\\+]+$/.test(trimmed)) {
    throw new Error(`${context} contains unsafe UBT argument characters.`);
  }
}

function validateUbtPositionalToken(token: string, context: string): void {
  validateUbtArgumentToken(token, context);
  const trimmed = token.trim();
  if (/^[-/@]/.test(trimmed) || trimmed.includes('=') || trimmed.includes(':') || trimmed.includes('/') || trimmed.includes('\\')) {
    throw new Error(`${context} must be a positional UBT token and cannot be a switch or path.`);
  }
}

function validateUbtTarget(target: string): void {
  validateUbtPositionalToken(target, 'run_ubt.target');
}

function validateUbtPlatform(platform: string): void {
  validateUbtPositionalToken(platform, 'run_ubt.platform');
  if (!ALLOWED_UBT_PLATFORMS.has(platform)) {
    throw new Error(`run_ubt.platform is not allowed: ${platform}`);
  }
}

function validateUbtConfiguration(configuration: string): void {
  validateUbtPositionalToken(configuration, 'run_ubt.configuration');
  if (!ALLOWED_UBT_CONFIGURATIONS.has(configuration)) {
    throw new Error(`run_ubt.configuration is not allowed: ${configuration}`);
  }
}

function getUbtOptionName(token: string): string | undefined {
  const trimmed = token.trim().toLowerCase();
  if (!trimmed.startsWith('-') && !trimmed.startsWith('/')) {
    return undefined;
  }

  const withoutPrefix = trimmed.replace(/^[-/]+/, '');
  const separatorIndex = withoutPrefix.search(/[=:]/);
  return separatorIndex >= 0 ? withoutPrefix.slice(0, separatorIndex) : withoutPrefix;
}

function validateUbtExtraArgumentToken(token: string): void {
  const trimmed = token.trim();
  if (trimmed.startsWith('@')) {
    throw new Error('UBT response-file arguments are blocked for safety.');
  }
  validateUbtArgumentToken(token, 'run_ubt.arguments');

  const optionName = getUbtOptionName(trimmed);
  if (optionName && BLOCKED_UBT_OVERRIDE_OPTIONS.has(optionName)) {
    throw new Error(`UBT argument ${optionName} cannot override the managed invocation.`);
  }
}

function validateUatOperation(operation: string): void {
  if (!ALLOWED_UAT_OPERATIONS.has(operation)) {
    throw new Error(`run_uat.uatOperation is not allowed: ${operation}`);
  }
}

function validateUatArgumentsString(extraArgs: string): void {
  validateUbtArgumentsString(extraArgs);
  for (const token of tokenizeArgs(extraArgs)) {
    const optionName = getUbtOptionName(token);
    if (optionName && new Set(['project', 'platform', 'clientconfig', 'serverconfig', 'archivedirectory']).has(optionName)) {
      throw new Error(`run_uat argument ${optionName} cannot override the managed invocation`);
    }
  }
}

async function findRunUatScript(): Promise<string | undefined> {
  const configured = process.env.UE_ENGINE_PATH ?? process.env.UNREAL_ENGINE_PATH;
  const engineRoot = configured
    ? (isEngineDirectoryPath(configured) ? configured : path.join(configured, 'Engine'))
    : undefined;
  const candidates = engineRoot
    ? [
      path.join(engineRoot, 'Build', 'BatchFiles', process.platform === 'win32' ? 'RunUAT.bat' : 'RunUAT.sh'),
      path.join(engineRoot, 'Build', 'BatchFiles', 'RunUAT.bat'),
      path.join(engineRoot, 'Build', 'BatchFiles', 'RunUAT.sh')
    ]
    : [];
  for (const candidate of candidates) {
    try {
      await fs.promises.access(candidate, fs.constants.F_OK);
      return candidate;
    } catch { /* try next candidate */ }
  }
  return undefined;
}

function resolveProjectFile(projectPath: string): string {
  if (projectPath.toLowerCase().endsWith('.uproject')) return projectPath;
  throw new Error('projectPath must point to a .uproject file for run_uat.');
}

async function collectReleaseFiles(root: string, current: string = root, output: string[] = []): Promise<string[]> {
  if (output.length >= 10000) return output;
  const entries = await fs.promises.readdir(current, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(current, entry.name);
    if (entry.isDirectory()) {
      await collectReleaseFiles(root, fullPath, output);
    } else if (entry.isFile()) {
      output.push(path.relative(root, fullPath).replace(/\\/g, '/'));
    }
    if (output.length >= 10000) break;
  }
  return output;
}

async function validateReleaseArtifact(args: PipelineArgs): Promise<Record<string, unknown>> {
  const archiveDirectory = typeof args.archiveDirectory === 'string' ? args.archiveDirectory.trim() : '';
  if (!archiveDirectory) {
    return { success: false, error: 'INVALID_ARGUMENT', message: 'archiveDirectory is required for validate_release' };
  }
  let stat: Awaited<ReturnType<typeof fs.promises.stat>>;
  try {
    stat = await fs.promises.stat(archiveDirectory);
  } catch {
    return { success: false, error: 'RELEASE_NOT_FOUND', message: `Release archive directory not found: ${archiveDirectory}`, archiveDirectory };
  }
  if (!stat.isDirectory()) {
    return { success: false, error: 'INVALID_ARGUMENT', message: 'archiveDirectory must be a directory', archiveDirectory };
  }

  if (args.jobId) {
    const job = jobManager.get(args.jobId);
    if (!job) return { success: false, error: 'JOB_NOT_FOUND', message: `Job not found: ${args.jobId}`, jobId: args.jobId };
    if (job.status !== 'completed') {
      return { success: false, error: 'RELEASE_NOT_READY', message: `Build job is ${job.status}`, jobId: args.jobId, status: job.status };
    }
  }

  const files = await collectReleaseFiles(archiveDirectory);
  const requiredFiles = Array.isArray(args.requiredFiles) ? args.requiredFiles : [];
  const invalidRequiredFiles = requiredFiles.filter(file => typeof file !== 'string' || !file.trim() || path.isAbsolute(file) || file.split(/[\\/]/).includes('..'));
  if (invalidRequiredFiles.length > 0) {
    return { success: false, error: 'INVALID_ARGUMENT', message: 'requiredFiles must contain safe relative paths', invalidRequiredFiles };
  }
  const missingFiles = requiredFiles.filter(file => !files.includes(file.replace(/\\/g, '/')));
  const pakFiles = files.filter(file => file.toLowerCase().endsWith('.pak'));
  const requirePak = args.requirePak === true;
  const errors: string[] = [];
  if (missingFiles.length > 0) errors.push(`Missing required files: ${missingFiles.join(', ')}`);
  if (requirePak && pakFiles.length === 0) errors.push('No .pak file was found in the release archive.');
  return cleanObject({
    success: errors.length === 0,
    error: errors.length === 0 ? undefined : 'RELEASE_VALIDATION_FAILED',
    message: errors.length === 0 ? 'Release artifact validation passed' : errors.join(' '),
    archiveDirectory,
    fileCount: files.length,
    pakFiles,
    missingFiles,
    requiredFiles,
    checks: { archiveDirectory: true, requiredFiles: missingFiles.length === 0, pak: !requirePak || pakFiles.length > 0 }
  });
}

function isPathInside(root: string, candidate: string): boolean {
  const relative = path.relative(path.resolve(root), path.resolve(candidate));
  return relative === '' || (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative));
}

async function signRelease(args: PipelineArgs): Promise<Record<string, unknown>> {
  const platform = args.platform || 'Win64';
  const artifactInput = typeof args.artifactPath === 'string' ? args.artifactPath.trim() : '';
  if (!artifactInput) return { success: false, error: 'INVALID_ARGUMENT', message: 'artifactPath is required for sign_release' };
  if (!ALLOWED_UBT_PLATFORMS.has(platform)) return { success: false, error: 'UNSUPPORTED_PLATFORM', message: `Unsupported signing platform: ${platform}`, platform };

  const projectInput = args.projectPath || process.env.UE_PROJECT_PATH;
  const projectRoot = projectInput ? path.resolve(projectInput.toLowerCase().endsWith('.uproject') ? path.dirname(projectInput) : projectInput) : undefined;
  const archiveRoot = args.archiveDirectory ? path.resolve(args.archiveDirectory) : projectRoot;
  if (!archiveRoot) return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'projectPath or archiveDirectory is required to constrain artifactPath' };
  const artifactPath = path.isAbsolute(artifactInput) ? path.resolve(artifactInput) : path.resolve(archiveRoot, artifactInput);
  if (!isPathInside(archiveRoot, artifactPath)) return { success: false, error: 'PATH_SECURITY_VIOLATION', message: 'artifactPath must remain inside archiveDirectory/projectPath', artifactPath };
  const artifactStat = await fs.promises.stat(artifactPath).catch(() => undefined);
  if (!artifactStat) return { success: false, error: 'ARTIFACT_NOT_FOUND', message: `Signing artifact not found: ${artifactPath}`, artifactPath };

  let executable: string | undefined;
  let commandArgs: string[] = [];
  if (platform === 'Win64') {
    executable = process.env.SIGNTOOL_PATH;
    if (!executable) {
      const probe = await execAsync('where signtool.exe').catch(() => undefined);
      executable = probe?.stdout.split(/\r?\n/).map(line => line.trim()).find(Boolean);
    }
    const certificate = args.certificatePath?.trim();
    const identity = args.signingIdentity?.trim();
    if (!certificate && !identity) return { success: false, error: 'SIGNING_IDENTITY_REQUIRED', message: 'Win64 signing requires certificatePath or signingIdentity thumbprint' };
    commandArgs = ['sign', '/fd', 'SHA256'];
    if (certificate) commandArgs.push('/f', certificate);
    else commandArgs.push('/sha1', identity as string);
    commandArgs.push(artifactPath);
  } else if (platform === 'Mac' || platform === 'IOS') {
    executable = process.env.CODESIGN_PATH || 'codesign';
    const identity = args.signingIdentity?.trim();
    if (!identity) return { success: false, error: 'SIGNING_IDENTITY_REQUIRED', message: `${platform} signing requires signingIdentity` };
    commandArgs = ['--force', '--deep', '--sign', identity, artifactPath];
  } else if (platform === 'Android') {
    executable = process.env.JARSIGNER_PATH || 'jarsigner';
    const keystore = args.keystorePath?.trim();
    const alias = args.signingAlias?.trim();
    if (!keystore || !alias) return { success: false, error: 'SIGNING_IDENTITY_REQUIRED', message: 'Android signing requires keystorePath and signingAlias' };
    if (args.signingPasswordEnv && !SIGNING_PASSWORD_ENV.test(args.signingPasswordEnv)) return { success: false, error: 'INVALID_ARGUMENT', message: 'signingPasswordEnv must be a safe environment variable name' };
    commandArgs = ['-keystore', keystore, artifactPath, alias];
  } else {
    return { success: false, error: 'SIGNING_NOT_SUPPORTED', message: `Signing is not implemented for ${platform}`, platform };
  }
  if (!executable) return { success: false, error: 'SIGNING_TOOL_NOT_FOUND', message: `No signing tool found for ${platform}`, platform };
  const dryRun = args.dryRun === true;
  const result = { success: true, dryRun, platform, artifactPath, executable, arguments: commandArgs.map(value => value === artifactPath ? '<artifact>' : value) };
  if (dryRun) return result;

  const childEnv = { ...process.env };
  if (args.signingPasswordEnv && childEnv[args.signingPasswordEnv] === undefined) {
    return { success: false, error: 'SIGNING_SECRET_MISSING', message: `Signing password environment variable is not set: ${args.signingPasswordEnv}` };
  }
  const child = spawn(executable, commandArgs, { shell: false, env: childEnv });
  const job = jobManager.startProcess({ label: `sign_release:${platform}`, process: child });
  if (args.async === true) return { ...result, started: true, jobId: job.jobId, status: job.status };
  return await new Promise(resolve => {
    child.once('close', code => resolve({ ...result, success: code === 0, error: code === 0 ? undefined : 'SIGNING_FAILED', exitCode: code, jobId: job.jobId }));
    child.once('error', error => resolve({ ...result, success: false, error: 'SPAWN_FAILED', message: error.message, jobId: job.jobId }));
  });
}

async function runPackaged(args: PipelineArgs): Promise<Record<string, unknown>> {
  const artifactInput = typeof args.artifactPath === 'string' ? args.artifactPath.trim() : '';
  if (!artifactInput) return { success: false, error: 'INVALID_ARGUMENT', message: 'artifactPath is required for run_packaged' };
  const projectInput = args.projectPath || process.env.UE_PROJECT_PATH;
  const projectRoot = projectInput ? path.resolve(projectInput.toLowerCase().endsWith('.uproject') ? path.dirname(projectInput) : projectInput) : undefined;
  const archiveRoot = args.archiveDirectory ? path.resolve(args.archiveDirectory) : projectRoot;
  if (!archiveRoot) return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'projectPath or archiveDirectory is required to constrain artifactPath' };
  const artifactPath = path.isAbsolute(artifactInput) ? path.resolve(artifactInput) : path.resolve(archiveRoot, artifactInput);
  if (!isPathInside(archiveRoot, artifactPath)) return { success: false, error: 'PATH_SECURITY_VIOLATION', message: 'artifactPath must remain inside archiveDirectory/projectPath', artifactPath };
  const artifactStat = await fs.promises.stat(artifactPath).catch(() => undefined);
  if (!artifactStat) return { success: false, error: 'ARTIFACT_NOT_FOUND', message: `Packaged executable not found: ${artifactPath}`, artifactPath };
  if (!artifactStat.isFile()) return { success: false, error: 'INVALID_ARTIFACT', message: 'artifactPath must identify an executable file', artifactPath };
  const extraArgs = args.arguments || '';
  validateUbtArgumentsString(extraArgs);
  const commandArgs = tokenizeArgs(extraArgs);
  const dryRun = args.dryRun === true;
  const result = { success: true, dryRun, artifactPath, arguments: commandArgs };
  if (dryRun) return result;
  const child = spawn(artifactPath, commandArgs, { shell: false, cwd: path.dirname(artifactPath), env: process.env });
  const job = jobManager.startProcess({ label: `run_packaged:${path.basename(artifactPath)}`, process: child });
  if (args.async === true) return { ...result, started: true, jobId: job.jobId, status: job.status };
  return await new Promise(resolve => {
    child.once('close', code => resolve({ ...result, success: code === 0, error: code === 0 ? undefined : 'PACKAGED_RUNTIME_FAILED', exitCode: code, jobId: job.jobId }));
    child.once('error', error => resolve({ ...result, success: false, error: 'SPAWN_FAILED', message: error.message, jobId: job.jobId }));
  });
}

/** Split a UBT argument string into tokens, respecting quoted segments. */
function tokenizeArgs(extraArgs: string): string[] {
  if (!extraArgs) {
    return [];
  }

  const args: string[] = [];
  let current = '';
  let inQuotes = false;
  let escapeNext = false;

  for (let i = 0; i < extraArgs.length; i++) {
    const ch = extraArgs[i];

    if (escapeNext) {
      current += ch;
      escapeNext = false;
      continue;
    }

    if (ch === '\\') {
      escapeNext = true;
      continue;
    }

    if (ch === '"') {
      inQuotes = !inQuotes;
      continue;
    }

    if (!inQuotes && /\s/.test(ch)) {
      if (current.length > 0) {
        args.push(current);
        current = '';
      }
    } else {
      current += ch;
    }
  }

  if (current.length > 0) {
    args.push(current);
  }

  return args;
}

/** Return true only when the final path segment is literally "Engine". */
function isEngineDirectoryPath(enginePath: string): boolean {
  const trimmed = enginePath.replace(/[\\/]+$/, '');
  const segments = trimmed.split(/[\\/]/);
  const lastSegment = segments[segments.length - 1];
  return typeof lastSegment === 'string' && lastSegment.toLowerCase() === 'engine';
}

/**
 * Probe a concrete UBT file path for existence + executability.
 * Returns the path if valid, undefined otherwise.
 */
async function tryUbtpath(candidate: string): Promise<string | undefined> {
  let mode = fs.constants.F_OK;
  if (process.platform !== 'win32') {
    // For non-Windows, require X_OK unless it's a .dll which dotnet executes
    if (!candidate.endsWith('.dll')) {
      mode = fs.constants.F_OK | fs.constants.X_OK;
    }
  }
  try {
    await fs.promises.access(candidate, mode);
    return candidate;
  } catch { /* not usable */ }
  return undefined;
}

/**
 * Resolve the UnrealBuildTool executable path using multiple discovery strategies.
 * Returns an empty string when not found — caller should delegate to C++ handler.
 */
async function findUbtExecutable(): Promise<string> {
  // ─── Strategy 1: Explicit environment variable ────────────────────────
  // UE_ENGINE_PATH is the convention in this project (see AGENTS.md / README).
  // The path may point to either:
  //   • .../UE_5.x/Engine    (already includes "Engine" suffix)
  //   • .../UE_5.x           (root directory without "Engine")
  const enginePath =
    process.env.UE_ENGINE_PATH ??
    process.env.UNREAL_ENGINE_PATH ??
    undefined;

  if (enginePath) {
    const endsWithEngine = isEngineDirectoryPath(enginePath);

    const roots: string[] = endsWithEngine
      ? [enginePath]
      : [path.join(enginePath, 'Engine')];

    for (const root of roots) {
      // Check all known UBT locations across UE 5.0 – 5.7:
      //   1. UE 5.4+ wrapper .exe  (Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe)
      //   2. UE 5.4+ directory .exe (Binaries/DotNET/UnrealBuildTool) — some versions
      //   3. Pre-5.4 .dll          (Binaries/DotNET/UnrealBuildTool.dll)
      const candidates = [
        path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe'),
        path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool'),
        path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.exe'),
        path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.dll'),
      ];
      for (const c of candidates) {
        const hit = await tryUbtpath(c);
        if (hit) return hit;
      }
    }
  }

  // ─── Strategy 2: Discover from .uproject EngineAssociation ────────────
  const projectPath = process.env.UE_PROJECT_PATH;
  if (projectPath) {
    let uprojectFile: string | undefined = undefined;
    if (projectPath.endsWith('.uproject')) {
      uprojectFile = projectPath;
    } else {
      try {
        const files = await fs.promises.readdir(projectPath);
        const found = files.find(f => f.endsWith('.uproject'));
        if (found) uprojectFile = path.join(projectPath, found);
      } catch { /* ignore */ }
    }

    if (uprojectFile) {
      try {
        const contentRaw = await fs.promises.readFile(uprojectFile, 'utf-8');
        const content = JSON.parse(contentRaw);
        const association = content.EngineAssociation as string | undefined;

        if (association) {
          const versionMatch = association.match(/^(\d+)\.(\d+)$/);
          if (versionMatch) {
            const [, major, minor] = versionMatch;
            const versionKey = `UE_${major}.${minor}`;

            const searchRoots: string[] = [];

            // Version-specific env vars
            const versionedEnvVars = [
              `${versionKey}_ROOT`,
              `${versionKey.replace('.', '_')}_ROOT`,
              `UE_ENGINE_PATH_${major}${minor}`,
              `UE${major}${minor}_ENGINE_PATH`,
            ];
            for (const key of versionedEnvVars) {
              const value = process.env[key];
              if (value) searchRoots.push(value);
            }

            // Standard Epic Launcher (Windows)
            searchRoots.push(
              path.join('C:', 'Program Files', 'Epic Games', versionKey, 'Engine'),
              path.join('E:', 'EpicGames', versionKey, 'Engine'),
            );

            // Known custom install layouts from this machine
            searchRoots.push(
              path.join('X:', 'Unreal_Engine', versionKey, 'Engine'),
              path.join('D:', 'Unreal_Engine', versionKey, 'Engine'),
            );

            for (const root of searchRoots) {
              if (!root) continue;
              const candidates = [
                path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe'),
                path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool'),
                path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.exe'),
                path.join(root, 'Binaries', 'DotNET', 'UnrealBuildTool.dll'),
              ];
              for (const c of candidates) {
                const hit = await tryUbtpath(c);
                if (hit) return hit;
              }
            }
          }
        }

        // Fallback: check DefaultEngine.ini for EnginePath
        const iniPath = path.join(path.dirname(uprojectFile), 'Config', 'DefaultEngine.ini');
        try {
          const iniContent = await fs.promises.readFile(iniPath, 'utf-8');
          const iniMatch = iniContent.match(/EnginePath\s*=\s*(.+)/);
          if (iniMatch) {
            const iniEnginePath = iniMatch[1].trim().replace(/["']/g, '');
            const candidates = [
              path.join(iniEnginePath, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe'),
              path.join(iniEnginePath, 'Binaries', 'DotNET', 'UnrealBuildTool.exe'),
              path.join(iniEnginePath, 'Binaries', 'DotNET', 'UnrealBuildTool.dll'),
            ];
            for (const c of candidates) {
              const hit = await tryUbtpath(c);
              if (hit) return hit;
            }
          }
        } catch { /* no ini */ }
      } catch { /* uproject parse failed */ }
    }
  }

  // ─── Strategy 3: Global PATH lookup ───────────────────────────────────
  try {
    const whichCmd = process.platform === 'win32' ? 'where' : 'which';
    const { stdout } = await execAsync(`${whichCmd} UnrealBuildTool`, {
      encoding: 'utf-8',
      timeout: 5000,
    });
    if (stdout) {
      const first = stdout.trim().split(/\r?\n/)[0];
      if (first) return first;
    }
  } catch { /* not on PATH */ }

  // Not found — caller will delegate to the C++ bridge handler.
  return '';
}

/** Return Unreal's bundled .NET runtime folder for the current platform, if present. */
async function findBundledDotNetRoot(ubtPath: string): Promise<string | undefined> {
  const platformFolder = (() => {
    if (process.platform === 'win32') {
      return process.arch === 'arm64' ? 'win-arm64' : 'win-x64';
    }
    if (process.platform === 'darwin') {
      return process.arch === 'arm64' ? 'mac-arm64' : 'mac-x64';
    }
    return process.arch === 'arm64' ? 'linux-arm64' : 'linux-x64';
  })();

  let candidateRoot = path.dirname(ubtPath);
  for (let depth = 0; depth < 6; depth++) {
    const dotNetBase = path.join(candidateRoot, 'Binaries', 'ThirdParty', 'DotNet');

    try {
      const entries = await fs.promises.readdir(dotNetBase, { withFileTypes: true });
      const versionDirs = entries
        .filter(entry => entry.isDirectory())
        .map(entry => entry.name)
        .sort((a, b) => b.localeCompare(a, undefined, { numeric: true }));

      for (const versionDir of versionDirs) {
        const runtimeRoot = path.join(dotNetBase, versionDir, platformFolder);
        const dotnetExecutable = path.join(runtimeRoot, process.platform === 'win32' ? 'dotnet.exe' : 'dotnet');
        const hit = await tryUbtpath(dotnetExecutable);
        if (hit) {
          return runtimeRoot;
        }
      }
    } catch { }

    const parent = path.dirname(candidateRoot);
    if (parent === candidateRoot) {
      break;
    }
    candidateRoot = parent;
  }

  return undefined;
}

/** Dispatch system_control pipeline actions to local UBT or the C++ bridge. */
export async function handlePipelineTools(action: string, args: PipelineArgs, tools: ITools) {
  switch (action) {
    case 'sign_release':
      return signRelease(args);
    case 'run_packaged':
      return runPackaged(args);
    case 'validate_release':
      return validateReleaseArtifact(args);
    case 'run_uat': {
      const operation = args.uatOperation || 'build_cook_stage_package';
      const platform = args.platform || 'Win64';
      const configuration = args.configuration || 'Development';
      const extraArgs = args.arguments || '';
      validateUatOperation(operation);
      validateUbtPlatform(platform);
      validateUbtConfiguration(configuration);
      validateUatArgumentsString(extraArgs);

      const script = await findRunUatScript();
      if (!script) {
        throw new Error('RunUAT was not found. Set UE_ENGINE_PATH to an Unreal Engine root or Engine directory.');
      }
      const projectPath = args.projectPath || process.env.UE_PROJECT_PATH;
      if (!projectPath) throw new Error('UE_PROJECT_PATH or projectPath is required for run_uat.');
      const projectFile = resolveProjectFile(projectPath);
      const archiveDirectory = args.archiveDirectory || path.join(path.dirname(projectFile), 'Saved', 'MCPBuilds');
      const serverBuild = args.server === true || operation.endsWith('_server');
      const baseOperation = serverBuild ? operation.replace(/_server$/, '') : operation;
      const buildCookRunArgs = ['BuildCookRun', `-project=${projectFile}`, '-noP4', `-platform=${platform}`, `-clientconfig=${configuration}`];
      if (serverBuild) buildCookRunArgs.push('-server', '-noclient', `-serverconfig=${args.serverConfiguration || configuration}`);
      if (baseOperation === 'build' || baseOperation === 'build_cook_stage_package') buildCookRunArgs.push('-build');
      if (baseOperation === 'cook' || baseOperation === 'stage' || baseOperation === 'package' || baseOperation === 'archive' || baseOperation === 'build_cook_stage_package') buildCookRunArgs.push('-cook');
      if (baseOperation === 'stage' || baseOperation === 'package' || baseOperation === 'archive' || baseOperation === 'build_cook_stage_package') buildCookRunArgs.push('-stage');
      if (baseOperation === 'package' || baseOperation === 'archive' || baseOperation === 'build_cook_stage_package') buildCookRunArgs.push('-pak');
      if (baseOperation === 'archive' || baseOperation === 'build_cook_stage_package') buildCookRunArgs.push('-archive', `-archivedirectory=${archiveDirectory}`);
      buildCookRunArgs.push(...tokenizeArgs(extraArgs));

      const executable = process.platform === 'win32' ? 'cmd.exe' : 'bash';
      const actualArgs = process.platform === 'win32' ? ['/d', '/s', '/c', script, ...buildCookRunArgs] : [script, ...buildCookRunArgs];
      const child = spawn(executable, actualArgs, { shell: false });
      const job = jobManager.startProcess({ label: `run_uat:${operation}/${platform}/${configuration}`, process: child });
      if (args.async === true) {
        return cleanObject({ success: true, started: true, jobId: job.jobId, status: job.status, operation, server: serverBuild, archiveDirectory });
      }
      return new Promise(resolve => {
        child.once('close', code => resolve(cleanObject({
          success: code === 0,
          error: code === 0 ? undefined : 'UAT_FAILED',
          message: code === 0 ? 'RunUAT completed successfully' : `RunUAT failed with code ${code}`,
          operation,
          archiveDirectory,
          jobId: job.jobId
        })));
        child.once('error', error => resolve({ success: false, error: 'SPAWN_FAILED', message: error.message, jobId: job.jobId }));
      });
    }
    case 'run_ubt': {
      const target = args.target;
      const platform = args.platform || 'Win64';
      const configuration = args.configuration || 'Development';
      const extraArgs = args.arguments || '';

      if (!target) {
        throw new Error('Target is required for run_ubt');
      }

      validateUbtTarget(target);
      validateUbtPlatform(platform);
      validateUbtConfiguration(configuration);
      validateUbtArgumentsString(extraArgs);

      const discoveredUbtPath = await findUbtExecutable();

      if (!discoveredUbtPath) {
        // UBT not found on TS side — delegate to C++ handler which uses
        // FPaths::EngineDir() and always knows the correct engine root.
        const res = await executeAutomationRequest(
          tools,
          'manage_pipeline',
          { ...args, subAction: action },
          'Automation bridge not available for run_ubt'
        );
        return cleanObject(res);
      }

      let projectPath = process.env.UE_PROJECT_PATH;
      if (!projectPath && args.projectPath) {
        projectPath = args.projectPath;
      }

      if (!projectPath) {
        throw new Error('UE_PROJECT_PATH environment variable is not set and no projectPath argument was provided.');
      }

      let uprojectFile = projectPath;
      if (!uprojectFile.endsWith('.uproject')) {
        try {
          const files = await fs.promises.readdir(projectPath);
          const found = files.find(f => f.endsWith('.uproject'));
          if (found) {
            uprojectFile = path.join(projectPath, found);
          }
        } catch (_e) {
          throw new Error(`Could not read project directory: ${projectPath}`);
        }
      }

      const projectArg = `-Project=${uprojectFile}`;
      const extraTokens = tokenizeArgs(extraArgs);

      const cmdArgs = [
        target,
        platform,
        configuration,
        projectArg,
        ...extraTokens
      ];

      // UE 5.4+ ships UBT as a .dll invoked via `dotnet`; earlier versions
      // provide a standalone .exe wrapper.
      const isDll = discoveredUbtPath.endsWith('.dll');
      const executable = isDll ? 'dotnet' : discoveredUbtPath;
      const actualArgs = isDll ? [discoveredUbtPath, ...cmdArgs] : cmdArgs;
      const bundledDotNetRoot = await findBundledDotNetRoot(discoveredUbtPath);
      const childEnv = bundledDotNetRoot
        ? {
          ...process.env,
          DOTNET_ROOT: bundledDotNetRoot,
          DOTNET_MULTILEVEL_LOOKUP: '0',
          PATH: `${bundledDotNetRoot}${path.delimiter}${process.env.PATH ?? ''}`,
        }
        : process.env;

      if (args.async === true) {
        const child = spawn(executable, actualArgs, { shell: false, env: childEnv });
        const job = jobManager.startProcess({
          label: `run_ubt:${target}/${platform}/${configuration}`,
          process: child,
        });
        return cleanObject({
          success: true,
          started: true,
          jobId: job.jobId,
          status: job.status,
          command: `${executable} ${cmdArgs.map(arg => arg.includes(' ') ? `"${arg}"` : arg).join(' ')}`,
          message: 'UnrealBuildTool job started. Poll system_control with action get_job_status.',
        });
      }

      return new Promise((resolve) => {
        const child = spawn(executable, actualArgs, { shell: false, env: childEnv });

        const MAX_OUTPUT_SIZE = 20 * 1024; // 20KB cap
        let stdout = '';
        let stderr = '';

        child.stdout.on('data', (data) => {
          const str = data.toString();
          process.stderr.write(str);
          stdout += str;
          if (stdout.length > MAX_OUTPUT_SIZE) {
            stdout = stdout.substring(stdout.length - MAX_OUTPUT_SIZE);
          }
        });

        child.stderr.on('data', (data) => {
          const str = data.toString();
          process.stderr.write(str);
          stderr += str;
          if (stderr.length > MAX_OUTPUT_SIZE) {
            stderr = stderr.substring(stderr.length - MAX_OUTPUT_SIZE);
          }
        });

        child.on('close', (code) => {
          const truncatedNote = (stdout.length >= MAX_OUTPUT_SIZE || stderr.length >= MAX_OUTPUT_SIZE)
            ? '\n[Output truncated for response payload]'
            : '';

          const quotedArgs = cmdArgs.map(arg => arg.includes(' ') ? `"${arg}"` : arg);

          if (code === 0) {
            resolve({
              success: true,
              message: 'UnrealBuildTool finished successfully',
              output: stdout + truncatedNote,
              command: `${executable} ${quotedArgs.join(' ')}`
            });
          } else {
            resolve({
              success: false,
              error: 'UBT_FAILED',
              message: `UnrealBuildTool failed with code ${code}`,
              output: stdout + truncatedNote,
              errorOutput: stderr + truncatedNote,
              command: `${executable} ${quotedArgs.join(' ')}`
            });
          }
        });

        child.on('error', (err) => {
          const quotedArgs = cmdArgs.map(arg => arg.includes(' ') ? `"${arg}"` : arg);

          resolve({
            success: false,
            error: 'SPAWN_FAILED',
            message: `Failed to spawn UnrealBuildTool: ${err.message}`,
            command: `${executable} ${quotedArgs.join(' ')}`
          });
        });
      });
    }

    default: {
      return cleanObject({ success: false, error: 'UNKNOWN_ACTION', message: `Unknown system_control pipeline action: ${action}` });
    }
  }
}
