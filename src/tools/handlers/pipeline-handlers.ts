import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { PipelineArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { spawn, exec, execFile } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import util from 'node:util';
import { jobManager } from '../../services/job-manager.js';
import { validateGameArchitecture } from '../../services/game-architecture-service.js';
import { createHash } from 'node:crypto';
import { manageProjectPlugins } from '../../services/project-plugin-service.js';
import { runUnrealAutomationTests, validateProject } from '../../services/project-validation-service.js';

/** Promisified child_process.exec for async shell commands. */
const execAsync = util.promisify(exec);
const execFileAsync = util.promisify(execFile);
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
  const requiredDirectories = Array.isArray(args.requiredDirectories) ? args.requiredDirectories : [];
  const invalidRequiredDirectories = requiredDirectories.filter(directory => typeof directory !== 'string' || !directory.trim() || path.isAbsolute(directory) || directory.split(/[\\/]/).includes('..'));
  if (invalidRequiredDirectories.length > 0) {
    return { success: false, error: 'INVALID_ARGUMENT', message: 'requiredDirectories must contain safe relative paths', invalidRequiredDirectories };
  }
  const missingDirectories: string[] = [];
  for (const directory of requiredDirectories) {
    const directoryPath = path.resolve(archiveDirectory, directory);
    const directoryStat = await fs.promises.stat(directoryPath).catch(() => undefined);
    if (!directoryStat?.isDirectory()) missingDirectories.push(directory.replace(/\\/g, '/'));
  }
  const pakFiles = files.filter(file => file.toLowerCase().endsWith('.pak'));
  const requirePak = args.requirePak === true;
  const errors: string[] = [];
  if (missingFiles.length > 0) errors.push(`Missing required files: ${missingFiles.join(', ')}`);
  if (missingDirectories.length > 0) errors.push(`Missing required directories: ${missingDirectories.join(', ')}`);
  if (requirePak && pakFiles.length === 0) errors.push('No .pak file was found in the release archive.');
  let manifestResult: Record<string, unknown> | undefined;
  if (args.manifestPath !== undefined) {
    const manifestInput = typeof args.manifestPath === 'string' ? args.manifestPath.trim() : '';
    const manifestAbsolute = path.resolve(archiveDirectory, manifestInput);
    if (!manifestInput || path.isAbsolute(manifestInput) || !isPathInside(archiveDirectory, manifestAbsolute)) {
      return { success: false, error: 'INVALID_ARGUMENT', message: 'manifestPath must be a safe relative path inside archiveDirectory' };
    }
    try {
      const parsed = JSON.parse(await fs.promises.readFile(manifestAbsolute, 'utf8')) as unknown;
      const entries = parsed && typeof parsed === 'object' && !Array.isArray(parsed) && 'files' in parsed
        ? (parsed as { files?: unknown }).files
        : parsed;
      if (!entries || typeof entries !== 'object' || Array.isArray(entries)) {
        return { success: false, error: 'INVALID_MANIFEST', message: 'manifest must be a JSON object mapping relative file paths to SHA-256 hashes' };
      }
      const mismatches: string[] = [];
      const manifestMissing: string[] = [];
      for (const [relativeFile, expectedHash] of Object.entries(entries)) {
        if (!/^[a-f0-9]{64}$/i.test(String(expectedHash)) || path.isAbsolute(relativeFile) || !isPathInside(archiveDirectory, path.resolve(archiveDirectory, relativeFile))) {
          return { success: false, error: 'INVALID_MANIFEST', message: `Invalid manifest entry: ${relativeFile}` };
        }
        const target = path.resolve(archiveDirectory, relativeFile);
        try {
          const digest = createHash('sha256').update(await fs.promises.readFile(target)).digest('hex');
          if (digest.toLowerCase() !== String(expectedHash).toLowerCase()) mismatches.push(relativeFile);
        } catch {
          manifestMissing.push(relativeFile);
        }
      }
      if (mismatches.length > 0) errors.push(`Manifest hash mismatch: ${mismatches.join(', ')}`);
      if (manifestMissing.length > 0) errors.push(`Manifest files missing: ${manifestMissing.join(', ')}`);
      manifestResult = { path: manifestInput.replace(/\\/g, '/'), entries: Object.keys(entries).length, mismatches, missingFiles: manifestMissing, valid: mismatches.length === 0 && manifestMissing.length === 0 };
    } catch (error) {
      return { success: false, error: 'INVALID_MANIFEST', message: `Unable to read release manifest: ${String(error)}` };
    }
  }
  return cleanObject({
    success: errors.length === 0,
    error: errors.length === 0 ? undefined : 'RELEASE_VALIDATION_FAILED',
    message: errors.length === 0 ? 'Release artifact validation passed' : errors.join(' '),
    archiveDirectory,
    fileCount: files.length,
    pakFiles,
    missingFiles,
    missingDirectories,
    requiredFiles,
    requiredDirectories,
    ...(manifestResult ? { manifest: manifestResult } : {}),
    checks: { archiveDirectory: true, requiredFiles: missingFiles.length === 0, requiredDirectories: missingDirectories.length === 0, pak: !requirePak || pakFiles.length > 0, manifest: manifestResult ? manifestResult.valid === true : true }
  });
}

async function runReleaseGate(args: PipelineArgs): Promise<Record<string, unknown>> {
  if (args.runAutomationTests === true && !args.projectPath) {
    return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'projectPath is required when runAutomationTests is enabled for release_gate' };
  }
  const artifact = await validateReleaseArtifact(args);
  const checks: Record<string, unknown> = { artifact };
  let valid = artifact.success === true;

  if (args.projectPath && args.runProjectValidation !== false) {
    const project = await validateProject({
      projectPath: args.projectPath,
      requiredFiles: args.projectRequiredFiles,
      requiredDirectories: args.projectRequiredDirectories,
      includeInventory: false,
      validationMode: 'static',
      enginePath: args.enginePath,
      timeoutMs: args.timeoutMs
    });
    checks.project = project;
    valid = valid && project.success === true;
  }

  if (args.projectPath && args.validateArchitecture === true) {
    let architecture: Record<string, unknown>;
    try {
      architecture = await validateGameArchitecture({
        projectPath: args.projectPath,
        manifestPath: args.architectureManifestPath || 'Config/Architecture/game.json',
        includeOptional: false
      });
    } catch (error) {
      architecture = { success: false, valid: false, error: 'ARCHITECTURE_MANIFEST_INVALID', message: error instanceof Error ? error.message : String(error) };
    }
    checks.architecture = architecture;
    valid = valid && architecture.success === true;
  }

  if (args.projectPath && args.validatePlugins !== false) {
    const plugins = await manageProjectPlugins(args.projectPath, 'validate');
    checks.plugins = plugins;
    valid = valid && plugins.success === true;
  }

  if (args.projectPath && args.runAutomationTests === true) {
    const tests = await runUnrealAutomationTests({
      projectPath: args.projectPath,
      enginePath: args.enginePath,
      filter: typeof args.filter === 'string' ? args.filter : undefined,
      test: args.testName,
      reportPath: args.reportPath,
      timeoutMs: args.timeoutMs
    });
    let terminalTests = tests;
    if (tests.success === true && typeof tests.jobId === 'string') {
      const terminal = await waitForTerminalHostJob(tests.jobId, getProcessTimeoutMs(args) ?? 30 * 60 * 1000);
      terminalTests = {
        ...tests,
        ...terminal,
        success: terminal.success === true
      };
    }
    checks.automationTests = terminalTests;
    valid = valid && terminalTests.success === true;
  }

  if (args.runPackagedSmoke === true) {
    if (!args.packagedArtifactPath) {
      const smoke = { success: false, error: 'PACKAGED_ARTIFACT_REQUIRED', message: 'packagedArtifactPath is required when runPackagedSmoke is enabled' };
      checks.packagedSmoke = smoke;
      valid = false;
    } else {
      const started = await runPackaged({ ...args, artifactPath: args.packagedArtifactPath, async: true });
      let terminalSmoke = started;
      if (started.success === true && typeof started.jobId === 'string') {
        terminalSmoke = await waitForTerminalHostJob(started.jobId, getProcessTimeoutMs(args) ?? 5 * 60 * 1000);
      }
      checks.packagedSmoke = terminalSmoke;
      valid = valid && terminalSmoke.success === true;
    }
  }

  return cleanObject({
    success: valid,
    error: valid ? undefined : 'RELEASE_GATE_FAILED',
    message: valid ? 'Release gate passed' : 'Release gate failed one or more checks',
    checks,
    passedChecks: Object.entries(checks).filter(([, value]) => (value as Record<string, unknown>).success === true).map(([name]) => name),
    failedChecks: Object.entries(checks).filter(([, value]) => (value as Record<string, unknown>).success !== true).map(([name]) => name)
  });
}

async function waitForTerminalHostJob(jobId: string, timeoutMs: number): Promise<Record<string, unknown>> {
  const deadline = Date.now() + Math.min(Math.max(timeoutMs, 1), 30 * 60 * 1000);
  while (Date.now() < deadline) {
    const snapshot = jobManager.get(jobId);
    if (!snapshot) return { success: false, error: 'JOB_NOT_FOUND', jobId };
    if (snapshot.status === 'completed' || snapshot.status === 'failed' || snapshot.status === 'cancelled') {
      return {
        ...snapshot,
        success: snapshot.status === 'completed' && snapshot.exitCode === 0,
        error: snapshot.status === 'completed' && snapshot.exitCode === 0 ? undefined : 'AUTOMATION_TESTS_FAILED'
      };
    }
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  jobManager.cancel(jobId);
  return { success: false, error: 'AUTOMATION_TEST_TIMEOUT', jobId, timeoutMs };
}

function isPathInside(root: string, candidate: string): boolean {
  const relative = path.relative(path.resolve(root), path.resolve(candidate));
  return relative === '' || (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative));
}

function getProcessTimeoutMs(args: PipelineArgs): number | undefined {
  return typeof args.timeoutMs === 'number' && Number.isFinite(args.timeoutMs) && args.timeoutMs > 0
    ? args.timeoutMs
    : undefined;
}

async function signRelease(args: PipelineArgs): Promise<Record<string, unknown>> {
  const processTimeoutMs = getProcessTimeoutMs(args);
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
  const job = jobManager.startProcess({ label: `sign_release:${platform}`, process: child, timeoutMs: processTimeoutMs });
  if (args.async === true) return { ...result, started: true, jobId: job.jobId, status: job.status };
  return await new Promise(resolve => {
    child.once('close', code => resolve({ ...result, success: code === 0, error: code === 0 ? undefined : 'SIGNING_FAILED', exitCode: code, jobId: job.jobId }));
    child.once('error', error => resolve({ ...result, success: false, error: 'SPAWN_FAILED', message: error.message, jobId: job.jobId }));
  });
}

async function runPackaged(args: PipelineArgs): Promise<Record<string, unknown>> {
  const processTimeoutMs = getProcessTimeoutMs(args);
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
  const job = jobManager.startProcess({ label: `run_packaged:${path.basename(artifactPath)}`, process: child, timeoutMs: processTimeoutMs });
  if (args.async === true) return { ...result, started: true, jobId: job.jobId, status: job.status };
  return await new Promise(resolve => {
    child.once('close', code => resolve({ ...result, success: code === 0, error: code === 0 ? undefined : 'PACKAGED_RUNTIME_FAILED', exitCode: code, jobId: job.jobId }));
    child.once('error', error => resolve({ ...result, success: false, error: 'SPAWN_FAILED', message: error.message, jobId: job.jobId }));
  });
}

async function findHostCommand(command: string): Promise<string | undefined> {
  try {
    const lookup = process.platform === 'win32' ? 'where.exe' : 'which';
    const result = await execFileAsync(lookup, [command], { windowsHide: true, timeout: 2000, maxBuffer: 16 * 1024 });
    return result.stdout.split(/\r?\n/).map(line => line.trim()).find(Boolean);
  } catch {
    return undefined;
  }
}

async function findInsightsCommand(): Promise<string | undefined> {
  const configured = process.env.UNREAL_INSIGHTS_PATH;
  const engineInput = process.env.UE_ENGINE_PATH || process.env.UNREAL_ENGINE_PATH;
  const engineRoot = engineInput ? (isEngineDirectoryPath(engineInput) ? engineInput : path.join(engineInput, 'Engine')) : undefined;
  const candidates = [
    configured,
    ...(engineRoot ? [
      path.join(engineRoot, 'Binaries', 'Win64', 'UnrealInsights.exe'),
      path.join(engineRoot, 'Binaries', 'Linux', 'UnrealInsights'),
      path.join(engineRoot, 'Binaries', 'Mac', 'UnrealInsights'),
      path.join(engineRoot, 'Binaries', 'Mac', 'UnrealInsights.app', 'Contents', 'MacOS', 'UnrealInsights')
    ] : [])
  ].filter((candidate): candidate is string => Boolean(candidate));
  for (const candidate of candidates) {
    if (path.isAbsolute(candidate) && await fs.promises.stat(candidate).then(stat => stat.isFile()).catch(() => false)) return candidate;
  }
  return findHostCommand(process.platform === 'win32' ? 'UnrealInsights.exe' : 'UnrealInsights');
}

async function analyzeTrace(args: PipelineArgs): Promise<Record<string, unknown>> {
  const traceInput = typeof args.tracePath === 'string' ? args.tracePath.trim() : '';
  if (!traceInput) return { success: false, error: 'INVALID_ARGUMENT', message: 'tracePath is required for analyze_trace' };
  const rootInput = args.archiveDirectory || args.projectPath || process.env.UE_PROJECT_PATH;
  if (!rootInput) return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'archiveDirectory, projectPath, or UE_PROJECT_PATH is required to constrain tracePath' };
  const root = path.resolve(rootInput.toLowerCase().endsWith('.uproject') ? path.dirname(rootInput) : rootInput);
  const tracePath = path.isAbsolute(traceInput) ? path.resolve(traceInput) : path.resolve(root, traceInput);
  if (!isPathInside(root, tracePath)) return { success: false, error: 'PATH_SECURITY_VIOLATION', message: 'tracePath must remain inside archiveDirectory/projectPath', tracePath };
  if (!/\.(utrace|trace)$/i.test(tracePath)) return { success: false, error: 'INVALID_TRACE_PATH', message: 'tracePath must identify a .utrace or .trace file', tracePath };
  const traceStat = await fs.promises.stat(tracePath).catch(() => undefined);
  if (!traceStat || !traceStat.isFile()) return { success: false, error: 'TRACE_NOT_FOUND', message: `Trace file not found: ${tracePath}`, tracePath };
  const dryRun = args.dryRun === true;
  const executable = dryRun ? undefined : await findInsightsCommand();
  if (!dryRun && !executable) return { success: false, error: 'UNREAL_INSIGHTS_NOT_FOUND', message: 'UnrealInsights executable was not found. Set UE_ENGINE_PATH or UNREAL_INSIGHTS_PATH.', tracePath };
  const commandArgs = [`-OpenTraceFile=${tracePath}`, '-AutoQuit', '-NoUI'];
  const result = { success: true, dryRun, tracePath, executable: executable || 'UnrealInsights', arguments: commandArgs.map(value => value === `-OpenTraceFile=${tracePath}` ? '-OpenTraceFile=<trace>' : value), analysisMode: 'open_and_validate' };
  if (dryRun) return result;
  const child = spawn(executable || 'UnrealInsights', commandArgs, { shell: false, cwd: path.dirname(tracePath), windowsHide: true });
  const job = jobManager.startProcess({ label: `analyze_trace:${path.basename(tracePath)}`, process: child, timeoutMs: getProcessTimeoutMs(args) });
  if (args.async === true) return { ...result, started: true, jobId: job.jobId, status: job.status };
  return await new Promise(resolve => {
    child.once('close', code => resolve({ ...result, success: code === 0, error: code === 0 ? undefined : 'TRACE_ANALYSIS_FAILED', exitCode: code, jobId: job.jobId }));
    child.once('error', error => resolve({ ...result, success: false, error: 'SPAWN_FAILED', message: error.message, jobId: job.jobId }));
  });
}

async function waitForPackagedServerStartup(
  jobId: string,
  readyPattern: RegExp | undefined,
  timeoutMs: number,
): Promise<{ ready: boolean; reason?: string }> {
  if (!readyPattern) {
    await new Promise(resolve => setTimeout(resolve, Math.min(timeoutMs, 250)));
    const snapshot = jobManager.get(jobId);
    return snapshot && (snapshot.status === 'running' || snapshot.status === 'queued')
      ? { ready: true }
      : { ready: false, reason: 'server_process_failed' };
  }
  return waitForJobOutputPattern(jobId, readyPattern, timeoutMs, 'server');
}

async function waitForJobOutputPattern(
  jobId: string,
  readyPattern: RegExp,
  timeoutMs: number,
  role: string,
): Promise<{ ready: boolean; reason?: string }> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const snapshot = jobManager.get(jobId);
    if (!snapshot || snapshot.status === 'failed' || snapshot.status === 'cancelled' || snapshot.status === 'completed') {
      return { ready: false, reason: `${role}_process_failed` };
    }
    readyPattern.lastIndex = 0;
    if (readyPattern.test(`${snapshot.output}\n${snapshot.errorOutput}`)) {
      return { ready: true };
    }
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  return { ready: false, reason: `${role}_ready_pattern_timeout` };
}

async function deployPackage(args: PipelineArgs): Promise<Record<string, unknown>> {
  const platform = (args.platform || '').trim();
  const isDesktop = platform === 'Win64' || platform === 'Linux' || platform === 'Mac';
  if (!isDesktop && platform !== 'Android' && platform !== 'IOS' && platform !== 'TVOS') {
    return { success: false, error: 'UNSUPPORTED_DEPLOYMENT_PLATFORM', message: 'deploy_package supports Win64, Linux, Mac, Android, IOS, and TVOS.' };
  }
  const artifactInput = typeof args.artifactPath === 'string' ? args.artifactPath.trim() : '';
  if (!artifactInput) return { success: false, error: 'INVALID_ARGUMENT', message: 'artifactPath is required for deploy_package' };
  const rootInput = args.archiveDirectory || args.projectPath || process.env.UE_PROJECT_PATH;
  if (!rootInput) return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'archiveDirectory, projectPath, or UE_PROJECT_PATH is required to constrain artifactPath' };
  const root = path.resolve(rootInput.toLowerCase().endsWith('.uproject') ? path.dirname(rootInput) : rootInput);
  const artifactPath = path.isAbsolute(artifactInput) ? path.resolve(artifactInput) : path.resolve(root, artifactInput);
  if (!isPathInside(root, artifactPath)) return { success: false, error: 'PATH_SECURITY_VIOLATION', message: 'artifactPath must remain inside archiveDirectory/projectPath', artifactPath };
  const artifactStat = await fs.promises.stat(artifactPath).catch(() => undefined);
  if (!artifactStat || (!artifactStat.isFile() && !(isDesktop && artifactStat.isDirectory()))) {
    return { success: false, error: 'ARTIFACT_NOT_FOUND', message: `Deployable artifact not found: ${artifactPath}`, artifactPath };
  }
  if (isDesktop) {
    const destinationInput = typeof args.destinationDirectory === 'string' ? args.destinationDirectory.trim() : '';
    if (!destinationInput) {
      return { success: false, error: 'DESTINATION_REQUIRED', message: 'destinationDirectory is required for desktop deployment.' };
    }
    const destinationDirectory = path.isAbsolute(destinationInput)
      ? path.resolve(destinationInput)
      : path.resolve(root, destinationInput);
    if (!isPathInside(root, destinationDirectory)) {
      return { success: false, error: 'PATH_SECURITY_VIOLATION', message: 'destinationDirectory must remain inside archiveDirectory/projectPath', destinationDirectory };
    }
    const destinationPath = path.join(destinationDirectory, path.basename(artifactPath));
    if (destinationPath === artifactPath || isPathInside(artifactPath, destinationPath)) {
      return { success: false, error: 'INVALID_ARGUMENT', message: 'destinationDirectory cannot be the artifact or one of its children.', destinationPath };
    }
    const destinationStat = await fs.promises.stat(destinationPath).catch(() => undefined);
    const overwrite = args.overwrite === true;
    if (destinationStat && !overwrite) {
      return { success: false, error: 'DESTINATION_EXISTS', message: `Deployment destination already exists: ${destinationPath}`, destinationPath };
    }
    const result = {
      success: true,
      dryRun: args.dryRun === true,
      platform,
      artifactPath,
      destinationDirectory,
      destinationPath,
      overwrite,
      command: 'local_copy',
      arguments: ['<artifact>', '<destination>']
    };
    if (result.dryRun) return result;
    const copyArtifact = async (signal: AbortSignal): Promise<void> => {
      if (signal.aborted) throw new Error('Deployment cancelled before copy started.');
      await fs.promises.mkdir(destinationDirectory, { recursive: true });
      await fs.promises.cp(artifactPath, destinationPath, { recursive: artifactStat.isDirectory(), force: overwrite, errorOnExist: !overwrite });
      if (signal.aborted) throw new Error('Deployment cancelled after copy completed.');
    };
    if (args.async === true) {
      const job = jobManager.startTask({ label: `deploy_package:${platform}/local`, task: copyArtifact, timeoutMs: getProcessTimeoutMs(args) });
      return { ...result, started: true, jobId: job.jobId, status: job.status };
    }
    await copyArtifact(new AbortController().signal);
    return result;
  }
  const deviceId = typeof args.deviceId === 'string' ? args.deviceId.trim() : '';
  if (!deviceId || !/^[A-Za-z0-9._:-]{1,128}$/.test(deviceId)) {
    return { success: false, error: 'INVALID_DEVICE_ID', message: 'deviceId is required and may contain only letters, numbers, dot, underscore, colon, and hyphen.' };
  }
  const command = platform === 'Android' ? 'adb' : 'xcrun';
  const commandArgs = platform === 'Android' ? ['-s', deviceId, 'install', '-r', artifactPath] : ['simctl', 'install', deviceId, artifactPath];
  const dryRun = args.dryRun === true;
  const toolPath = dryRun ? undefined : await findHostCommand(command);
  if (!dryRun && !toolPath) return { success: false, error: 'DEPLOYMENT_TOOL_NOT_FOUND', message: `${command} was not found on the host.`, platform };
  const result = { success: true, dryRun, platform, deviceId, artifactPath, command: toolPath || command, arguments: commandArgs.map(value => value === artifactPath ? '<artifact>' : value) };
  if (dryRun) return result;
  const child = spawn(toolPath || command, commandArgs, { shell: false, windowsHide: true });
  const job = jobManager.startProcess({ label: `deploy_package:${platform}/${deviceId}`, process: child, timeoutMs: getProcessTimeoutMs(args) });
  if (args.async === true) return { ...result, started: true, jobId: job.jobId, status: job.status };
  return await new Promise(resolve => {
    child.once('close', code => resolve({ ...result, success: code === 0, error: code === 0 ? undefined : 'DEPLOYMENT_FAILED', exitCode: code, jobId: job.jobId }));
    child.once('error', error => resolve({ ...result, success: false, error: 'SPAWN_FAILED', message: error.message, jobId: job.jobId }));
  });
}

async function runNetworkSoak(args: PipelineArgs): Promise<Record<string, unknown>> {
  const rootInput = args.archiveDirectory || args.projectPath || process.env.UE_PROJECT_PATH;
  if (!rootInput) return { success: false, error: 'PROJECT_PATH_REQUIRED', message: 'archiveDirectory, projectPath, or UE_PROJECT_PATH is required to constrain packaged artifacts' };
  const root = path.resolve(rootInput.toLowerCase().endsWith('.uproject') ? path.dirname(rootInput) : rootInput);
  const serverInput = typeof args.serverArtifactPath === 'string' ? args.serverArtifactPath.trim() : '';
  const clientInput = typeof args.clientArtifactPath === 'string' ? args.clientArtifactPath.trim() : '';
  if (!serverInput || !clientInput) return { success: false, error: 'INVALID_ARGUMENT', message: 'serverArtifactPath and clientArtifactPath are required for run_network_soak' };
  const resolveArtifact = async (input: string): Promise<{ path?: string; error?: Record<string, unknown> }> => {
    const artifactPath = path.isAbsolute(input) ? path.resolve(input) : path.resolve(root, input);
    if (!isPathInside(root, artifactPath)) return { error: { success: false, error: 'PATH_SECURITY_VIOLATION', message: 'Packaged artifacts must remain inside archiveDirectory/projectPath', artifactPath } };
    const stat = await fs.promises.stat(artifactPath).catch(() => undefined);
    if (!stat || !stat.isFile()) return { error: { success: false, error: 'ARTIFACT_NOT_FOUND', message: `Packaged executable not found: ${artifactPath}`, artifactPath } };
    return { path: artifactPath };
  };
  const server = await resolveArtifact(serverInput);
  if (server.error) return server.error;
  const client = await resolveArtifact(clientInput);
  if (client.error) return client.error;
  const clientCount = Number.isInteger(args.clientCount) ? args.clientCount as number : 2;
  if (clientCount < 1 || clientCount > 32) return { success: false, error: 'INVALID_CLIENT_COUNT', message: 'clientCount must be between 1 and 32' };
  const serverPort = Number.isInteger(args.serverPort) ? args.serverPort as number : 7777;
  if (serverPort < 1024 || serverPort > 65535) return { success: false, error: 'INVALID_SERVER_PORT', message: 'serverPort must be between 1024 and 65535' };
  const durationMs = Number.isFinite(args.durationMs) && (args.durationMs as number) > 0 ? Math.min(args.durationMs as number, 24 * 60 * 60 * 1000) : 60_000;
  const startupTimeoutMs = Number.isFinite(args.serverStartupTimeoutMs) && (args.serverStartupTimeoutMs as number) > 0
    ? Math.min(args.serverStartupTimeoutMs as number, 5 * 60 * 1000)
    : 30_000;
  const clientStartupTimeoutMs = Number.isFinite(args.clientStartupTimeoutMs) && (args.clientStartupTimeoutMs as number) > 0
    ? Math.min(args.clientStartupTimeoutMs as number, 5 * 60 * 1000)
    : startupTimeoutMs;
  const serverArguments = args.serverArguments || '';
  const clientArguments = args.clientArguments || '';
  validateUbtArgumentsString(serverArguments);
  validateUbtArgumentsString(clientArguments);
  const hasPortOverride = [...tokenizeArgs(serverArguments), ...tokenizeArgs(clientArguments)].some(token => getUbtOptionName(token) === 'port');
  if (hasPortOverride) return { success: false, error: 'INVALID_ARGUMENT', message: 'serverArguments and clientArguments cannot override the managed serverPort' };
  const serverArgs = [...tokenizeArgs(serverArguments), `-port=${serverPort}`];
  const clientArgs = [...tokenizeArgs(clientArguments), `127.0.0.1:${serverPort}`];
  let readyPattern: RegExp | undefined;
  if (args.serverReadyPattern !== undefined) {
    if (typeof args.serverReadyPattern !== 'string' || args.serverReadyPattern.trim().length === 0) {
      return { success: false, error: 'INVALID_SERVER_READY_PATTERN', message: 'serverReadyPattern must be a non-empty pattern when provided' };
    }
    try {
      readyPattern = new RegExp(args.serverReadyPattern.trim());
    } catch (error) {
      return { success: false, error: 'INVALID_SERVER_READY_PATTERN', message: error instanceof Error ? error.message : String(error) };
    }
  }
  let clientReadyPattern: RegExp | undefined;
  if (args.clientReadyPattern !== undefined) {
    if (typeof args.clientReadyPattern !== 'string' || args.clientReadyPattern.trim().length === 0) {
      return { success: false, error: 'INVALID_CLIENT_READY_PATTERN', message: 'clientReadyPattern must be a non-empty pattern when provided' };
    }
    try {
      clientReadyPattern = new RegExp(args.clientReadyPattern.trim());
    } catch (error) {
      return { success: false, error: 'INVALID_CLIENT_READY_PATTERN', message: error instanceof Error ? error.message : String(error) };
    }
  }
  const serverLaunch = { role: 'server', executable: server.path as string, arguments: serverArgs };
  const dryRun = args.dryRun === true;
  if (dryRun) {
    return {
      success: true,
      dryRun: true,
      serverPort,
      clientCount,
      durationMs,
      startupTimeoutMs,
      clientStartupTimeoutMs,
      serverReadyPattern: args.serverReadyPattern,
      clientReadyPattern: args.clientReadyPattern,
      launches: [serverLaunch, ...Array.from({ length: clientCount }, (_, index) => ({ role: `client-${index + 1}`, executable: client.path as string, arguments: clientArgs }))]
    };
  }
  const serverChild = spawn(serverLaunch.executable, serverLaunch.arguments, { shell: false, cwd: path.dirname(serverLaunch.executable), windowsHide: true });
  const serverJob = jobManager.startProcess({ label: `run_network_soak:${serverLaunch.role}`, process: serverChild, timeoutMs: durationMs });
  const startup = await waitForPackagedServerStartup(serverJob.jobId, readyPattern, startupTimeoutMs);
  if (!startup.ready) {
    jobManager.cancel(serverJob.jobId);
    return { success: false, error: 'SERVER_STARTUP_FAILED', message: `Packaged server did not become ready: ${startup.reason || 'unknown reason'}`, serverJobId: serverJob.jobId, startupTimeoutMs };
  }
  const jobs = [
    { ...serverLaunch, jobId: serverJob.jobId, status: serverJob.status, child: serverChild },
    ...Array.from({ length: clientCount }, (_, index) => {
      const launch = { role: `client-${index + 1}`, executable: client.path as string, arguments: clientArgs };
      const child = spawn(launch.executable, launch.arguments, { shell: false, cwd: path.dirname(launch.executable), windowsHide: true });
      const job = jobManager.startProcess({ label: `run_network_soak:${launch.role}`, process: child, timeoutMs: durationMs });
      return { ...launch, jobId: job.jobId, status: job.status, child };
    })
  ];
  const baseResult = {
    success: true,
    serverPort,
    clientCount,
    durationMs,
    startupTimeoutMs,
    clientStartupTimeoutMs,
    serverReadyPattern: args.serverReadyPattern,
    clientReadyPattern: args.clientReadyPattern,
    clientReadiness: undefined as Array<{ role: string; jobId: string; ready: boolean; reason?: string }> | undefined,
    serverArtifactPath: server.path,
    clientArtifactPath: client.path,
    jobs: jobs.map(({ child: _child, ...job }) => ({ ...job, arguments: job.arguments.map(value => value === '127.0.0.1:' + serverPort ? '<server>' : value) }))
  };
  if (clientReadyPattern) {
    const clientJobs = jobs.filter(job => job.role.startsWith('client-'));
    const readiness = await Promise.all(clientJobs.map(async job => ({
      role: job.role,
      jobId: job.jobId,
      ...(await waitForJobOutputPattern(job.jobId, clientReadyPattern, clientStartupTimeoutMs, job.role))
    })));
    const failedReadiness = readiness.filter(result => !result.ready);
    if (failedReadiness.length > 0) {
      for (const job of jobs) jobManager.cancel(job.jobId);
      return {
        ...baseResult,
        success: false,
        error: 'CLIENT_STARTUP_FAILED',
        message: 'One or more packaged clients did not emit the configured readiness pattern.',
        clientReadiness: readiness,
        cleanup: 'all_soak_jobs_cancelled'
      };
    }
    baseResult.clientReadiness = readiness;
  }
  if (args.async === true) return { ...baseResult, started: true, message: 'Network soak processes started; poll each returned jobId for terminal state.' };
  const outcomes = await Promise.all(jobs.map(job => new Promise<{ role: string; success: boolean; exitCode: number | null; jobId: string }>(resolve => {
    job.child.once('close', code => resolve({ role: job.role, success: code === 0, exitCode: code, jobId: job.jobId }));
    job.child.once('error', () => resolve({ role: job.role, success: false, exitCode: null, jobId: job.jobId }));
  })));
  return { ...baseResult, success: outcomes.every(outcome => outcome.success), outcomes };
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
    case 'generate_project_files':
      return handlePipelineTools('run_ubt', {
        ...args,
        target: 'ProjectFiles',
        arguments: [args.arguments, '-ProjectFiles'].filter(Boolean).join(' ')
      }, tools);
    case 'sign_release':
      return signRelease(args);
    case 'run_packaged':
      return runPackaged(args);
    case 'deploy_package':
      return deployPackage(args);
    case 'run_network_soak':
      return runNetworkSoak(args);
    case 'analyze_trace':
      return analyzeTrace(args);
    case 'validate_release':
      return validateReleaseArtifact(args);
    case 'release_gate':
      return runReleaseGate(args);
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
      if (args.compressed === true) buildCookRunArgs.push('-compressed');
      if (args.encryptIniFiles === true) buildCookRunArgs.push('-encryptinifiles');
      if (args.encryptPakIndex === true) buildCookRunArgs.push('-encryptpakindex');
      if (args.includePrerequisites === true) buildCookRunArgs.push('-prereqs');
      if (baseOperation === 'archive' || baseOperation === 'build_cook_stage_package') buildCookRunArgs.push('-archive', `-archivedirectory=${archiveDirectory}`);
      buildCookRunArgs.push(...tokenizeArgs(extraArgs));

      const executable = process.platform === 'win32' ? 'cmd.exe' : 'bash';
      const actualArgs = process.platform === 'win32' ? ['/d', '/s', '/c', script, ...buildCookRunArgs] : [script, ...buildCookRunArgs];
      const child = spawn(executable, actualArgs, { shell: false });
      const job = jobManager.startProcess({ label: `run_uat:${operation}/${platform}/${configuration}`, process: child, timeoutMs: getProcessTimeoutMs(args) });
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
          timeoutMs: getProcessTimeoutMs(args),
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
