import fs from 'node:fs/promises';
import path from 'node:path';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);

const TARGET_PLATFORMS = ['Win64', 'Linux', 'LinuxArm64', 'Mac', 'Android', 'IOS', 'TVOS', 'HoloLens', 'VisionOS'] as const;

async function existingFile(candidate: string): Promise<string | undefined> {
  try {
    const stats = await fs.stat(candidate);
    return stats.isFile() ? candidate : undefined;
  } catch {
    return undefined;
  }
}

async function findCommand(command: string): Promise<string | undefined> {
  try {
    const lookup = process.platform === 'win32' ? 'where.exe' : 'which';
    const result = await execFileAsync(lookup, [command], { windowsHide: true, timeout: 2000, maxBuffer: 16 * 1024 });
    const first = result.stdout.split(/\r?\n/).map((line) => line.trim()).find(Boolean);
    return first;
  } catch {
    return undefined;
  }
}

export async function inspectPlatformCapabilities(enginePath?: string): Promise<Record<string, unknown>> {
  const configured = enginePath || process.env.UE_ENGINE_PATH || process.env.UNREAL_ENGINE_PATH;
  const engineRoot = configured
    ? (path.basename(configured).toLowerCase() === 'engine' ? path.resolve(configured) : path.join(path.resolve(configured), 'Engine'))
    : undefined;
  const uatCandidates = engineRoot
    ? [
      path.join(engineRoot, 'Build', 'BatchFiles', 'RunUAT.bat'),
      path.join(engineRoot, 'Build', 'BatchFiles', 'RunUAT.sh')
    ]
    : [];
  const ubtCandidates = engineRoot
    ? [
      path.join(engineRoot, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.dll'),
      path.join(engineRoot, 'Binaries', 'DotNET', 'UnrealBuildTool', 'UnrealBuildTool.exe')
    ]
    : [];
  const [uatPath, ubtPath, signtool, codesign, jarsigner, apksigner] = await Promise.all([
    Promise.all(uatCandidates.map(existingFile)).then((paths) => paths.find(Boolean)),
    Promise.all(ubtCandidates.map(existingFile)).then((paths) => paths.find(Boolean)),
    findCommand('signtool'),
    findCommand('codesign'),
    findCommand('jarsigner'),
    findCommand('apksigner')
  ]);

  const hostPlatform = process.platform === 'win32' ? 'Win64' : process.platform === 'darwin' ? 'Mac' : 'Linux';
  return {
    success: true,
    hostPlatform,
    targetPlatforms: TARGET_PLATFORMS,
    engineConfigured: Boolean(engineRoot),
    engineRoot,
    runUatPath: uatPath,
    ubtPath,
    signingTools: {
      win64: signtool,
      mac: codesign,
      ios: codesign,
      android: jarsigner || apksigner,
      linux: undefined
    },
    deployableTargets: TARGET_PLATFORMS.filter((target) => target === hostPlatform || target === 'Linux' || target === 'LinuxArm64'),
    message: 'Platform capability discovery completed; SDK, certificates, and deployment credentials must still be configured per target.'
  };
}
