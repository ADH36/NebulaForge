import { describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { createHash } from 'node:crypto';
import type { ITools } from '../../types/tool-interfaces.js';
import type { PipelineArgs } from '../../types/handler-types.js';
import { handlePipelineTools } from './pipeline-handlers.js';

const tools = {} as unknown as ITools;

function runUbt(args: Partial<PipelineArgs>) {
  return handlePipelineTools('run_ubt', {
    target: 'MCPtestEditor',
    platform: 'Linux',
    configuration: 'Development',
    ...args
  } as PipelineArgs, tools);
}

function runUat(args: Partial<PipelineArgs>) {
  return handlePipelineTools('run_uat', {
    platform: 'Linux',
    configuration: 'Development',
    projectPath: 'Game.uproject',
    ...args
  } as PipelineArgs, tools);
}

function platformFolder(): string {
  if (process.platform === 'win32') return process.arch === 'arm64' ? 'win-arm64' : 'win-x64';
  if (process.platform === 'darwin') return process.arch === 'arm64' ? 'mac-arm64' : 'mac-x64';
  return process.arch === 'arm64' ? 'linux-arm64' : 'linux-x64';
}

function restoreEnv(key: string, value: string | undefined): void {
  if (value === undefined) {
    delete process.env[key];
  } else {
    process.env[key] = value;
  }
}

describe('handlePipelineTools run_ubt validation', () => {
  it('rejects switch-shaped positional fields before local or bridge execution', async () => {
    await expect(runUbt({ target: '-Project=/tmp/Evil.uproject' }))
      .rejects.toThrow(/positional UBT token/);
  });

  it('rejects platform and configuration values outside allowlists', async () => {
    await expect(runUbt({ platform: 'Windows' }))
      .rejects.toThrow(/platform is not allowed/);

    await expect(runUbt({ configuration: 'DevelopmentEditor' }))
      .rejects.toThrow(/configuration is not allowed/);
  });

  it('rejects extra arguments that override the managed invocation', async () => {
    await expect(runUbt({ arguments: '-Project=/tmp/Evil.uproject' }))
      .rejects.toThrow(/cannot override/);

    await expect(runUbt({ arguments: '--Project=/tmp/Evil.uproject' }))
      .rejects.toThrow(/cannot override/);

    await expect(runUbt({ arguments: '@/tmp/ubt.rsp' }))
      .rejects.toThrow(/response-file/);
  });

  it('uses Unreal bundled dotnet when UBT is discovered as the legacy dll', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'cosmic-wolf-ubt-'));
    const previousEnginePath = process.env.UE_ENGINE_PATH;
    const previousProjectPath = process.env.UE_PROJECT_PATH;
    const previousCapturePath = process.env.DOTNET_CAPTURE_PATH;

    try {
      const enginePath = path.join(tempRoot, 'Engine');
      const ubtPath = path.join(enginePath, 'Binaries', 'DotNET', 'UnrealBuildTool.dll');
      const dotnetRoot = path.join(enginePath, 'Binaries', 'ThirdParty', 'DotNet', '8.0.0', platformFolder());
      const dotnetPath = path.join(dotnetRoot, process.platform === 'win32' ? 'dotnet.exe' : 'dotnet');
      const projectPath = path.join(tempRoot, 'Game', 'MCPtest.uproject');
      const capturePath = path.join(tempRoot, 'dotnet-capture.json');

      await fs.mkdir(path.dirname(ubtPath), { recursive: true });
      await fs.mkdir(dotnetRoot, { recursive: true });
      await fs.mkdir(path.dirname(projectPath), { recursive: true });
      // A text file named dotnet.exe is not a runnable Windows executable.
      // Use Node itself as the launcher and make the fake DLL the JS entry
      // point so this test exercises the real spawn path on every platform.
      await fs.writeFile(ubtPath, [
        "const fs = require('node:fs');",
        'fs.writeFileSync(process.env.DOTNET_CAPTURE_PATH, JSON.stringify({',
        '  argv: process.argv.slice(1),',
        '  dotnetRoot: process.env.DOTNET_ROOT,',
        '  path: process.env.PATH',
        '}));'
      ].join('\n'));
      await fs.writeFile(projectPath, '{"FileVersion":3}');
      await fs.copyFile(process.execPath, dotnetPath);
      await fs.chmod(dotnetPath, 0o755);

      process.env.UE_ENGINE_PATH = enginePath;
      delete process.env.UE_PROJECT_PATH;
      process.env.DOTNET_CAPTURE_PATH = capturePath;

      const result = await runUbt({ projectPath });
      const capture = JSON.parse(await fs.readFile(capturePath, 'utf-8')) as Record<string, unknown>;

      expect(result).toMatchObject({ success: true });
      expect(capture.dotnetRoot).toBe(dotnetRoot);
      expect(capture.argv).toEqual(expect.arrayContaining([ubtPath, 'MCPtestEditor', 'Linux', 'Development']));
    } finally {
      restoreEnv('UE_ENGINE_PATH', previousEnginePath);
      restoreEnv('UE_PROJECT_PATH', previousProjectPath);
      restoreEnv('DOTNET_CAPTURE_PATH', previousCapturePath);
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });
});

describe('handlePipelineTools run_uat validation', () => {
  it('rejects unsupported operations before touching the engine or filesystem', async () => {
    await expect(runUat({ uatOperation: 'deploy' }))
      .rejects.toThrow(/uatOperation is not allowed/);
  });

  it('rejects managed BuildCookRun overrides in extra arguments', async () => {
    await expect(runUat({ arguments: '-project=Other.uproject' }))
      .rejects.toThrow(/cannot override/);
  });
});

describe('handlePipelineTools validate_release', () => {
  it('reports missing release artifacts and passes valid pak requirements', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-release-'));
    try {
      await fs.writeFile(path.join(tempRoot, 'Game-Win64-Shipping.pak'), 'pak');
      const valid = await handlePipelineTools('validate_release', {
        archiveDirectory: tempRoot,
        requiredFiles: ['Game-Win64-Shipping.pak'],
        requirePak: true
      } as PipelineArgs, tools);
      expect(valid).toMatchObject({ success: true, fileCount: 1, checks: { pak: true } });

      const invalid = await handlePipelineTools('validate_release', {
        archiveDirectory: tempRoot,
        requiredFiles: ['missing.exe'],
        requirePak: true
      } as PipelineArgs, tools);
      expect(invalid).toMatchObject({ success: false, error: 'RELEASE_VALIDATION_FAILED', missingFiles: ['missing.exe'] });
    } finally {
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });

  it('validates release manifest SHA-256 hashes', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-release-manifest-'));
    try {
      const artifact = 'Game-Win64-Shipping.pak';
      const content = 'pak-content';
      await fs.writeFile(path.join(tempRoot, artifact), content);
      const digest = createHash('sha256').update(content).digest('hex');
      await fs.writeFile(path.join(tempRoot, 'release-manifest.json'), JSON.stringify({ files: { [artifact]: digest } }));
      const valid = await handlePipelineTools('validate_release', { archiveDirectory: tempRoot, manifestPath: 'release-manifest.json' } as PipelineArgs, tools);
      expect(valid).toMatchObject({ success: true, checks: { manifest: true }, manifest: { valid: true, entries: 1 } });

      await fs.writeFile(path.join(tempRoot, artifact), 'tampered');
      const invalid = await handlePipelineTools('validate_release', { archiveDirectory: tempRoot, manifestPath: 'release-manifest.json' } as PipelineArgs, tools);
      expect(invalid).toMatchObject({ success: false, error: 'RELEASE_VALIDATION_FAILED', manifest: { valid: false, mismatches: [artifact] } });
    } finally {
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });
});

describe('handlePipelineTools release_gate', () => {
  it('aggregates artifact evidence into a shippability decision', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-release-gate-'));
    try {
      await fs.writeFile(path.join(tempRoot, 'Game-Win64-Shipping.pak'), 'pak');
      const result = await handlePipelineTools('release_gate', {
        archiveDirectory: tempRoot,
        requiredFiles: ['Game-Win64-Shipping.pak'],
        requirePak: true,
        runProjectValidation: false,
        validatePlugins: false
      } as PipelineArgs, tools);
      expect(result).toMatchObject({
        success: true,
        passedChecks: ['artifact'],
        failedChecks: [],
        checks: { artifact: { success: true } }
      });
    } finally {
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });
});

describe('handlePipelineTools deploy_package', () => {
  it('builds a confined Android deployment command in dry-run mode', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-deploy-'));
    try {
      const artifactPath = path.join(tempRoot, 'Game.apk');
      await fs.writeFile(artifactPath, 'apk');
      const result = await handlePipelineTools('deploy_package', {
        platform: 'Android',
        archiveDirectory: tempRoot,
        artifactPath: 'Game.apk',
        deviceId: 'emulator-5554',
        dryRun: true
      } as PipelineArgs, tools);
      expect(result).toMatchObject({ success: true, dryRun: true, platform: 'Android', command: 'adb', arguments: ['-s', 'emulator-5554', 'install', '-r', '<artifact>'] });
    } finally {
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });

  it('rejects deployment paths that escape the configured archive root', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-deploy-path-'));
    try {
      const result = await handlePipelineTools('deploy_package', {
        platform: 'Android',
        archiveDirectory: tempRoot,
        artifactPath: '../Game.apk',
        deviceId: 'emulator-5554',
        dryRun: true
      } as PipelineArgs, tools);
      expect(result).toMatchObject({ success: false, error: 'PATH_SECURITY_VIOLATION' });
    } finally {
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });
});

describe('handlePipelineTools run_network_soak', () => {
  it('rejects missing packaged server/client artifacts before launching processes', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-soak-'));
    try {
      const result = await handlePipelineTools('run_network_soak', {
        archiveDirectory: tempRoot,
        serverArtifactPath: 'Server.exe',
        clientArtifactPath: 'Client.exe',
        clientCount: 2,
        serverPort: 7777,
        durationMs: 5000
      } as PipelineArgs, tools);
      expect(result).toMatchObject({ success: false, error: 'ARTIFACT_NOT_FOUND' });
    } finally {
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });

  it('supports a side-effect-free dry run and validates readiness patterns', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-soak-dry-run-'));
    try {
      await fs.writeFile(path.join(tempRoot, 'Server.exe'), 'server');
      await fs.writeFile(path.join(tempRoot, 'Client.exe'), 'client');
      const result = await handlePipelineTools('run_network_soak', {
        archiveDirectory: tempRoot,
        serverArtifactPath: 'Server.exe',
        clientArtifactPath: 'Client.exe',
        clientCount: 2,
        serverPort: 7777,
        durationMs: 5000,
        serverStartupTimeoutMs: 1000,
        serverReadyPattern: 'LogNet: Ready',
        dryRun: true
      } as PipelineArgs, tools);
      expect(result).toMatchObject({ success: true, dryRun: true, startupTimeoutMs: 1000, serverReadyPattern: 'LogNet: Ready' });

      const invalid = await handlePipelineTools('run_network_soak', {
        archiveDirectory: tempRoot,
        serverArtifactPath: 'Server.exe',
        clientArtifactPath: 'Client.exe',
        serverReadyPattern: '[',
        dryRun: true
      } as PipelineArgs, tools);
      expect(invalid).toMatchObject({ success: false, error: 'INVALID_SERVER_READY_PATTERN' });
    } finally {
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });
});

describe('handlePipelineTools analyze_trace', () => {
  it('builds a confined headless UnrealInsights command in dry-run mode', async () => {
    const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-trace-'));
    try {
      await fs.writeFile(path.join(tempRoot, 'capture.utrace'), 'trace');
      const result = await handlePipelineTools('analyze_trace', {
        archiveDirectory: tempRoot,
        tracePath: 'capture.utrace',
        dryRun: true
      } as PipelineArgs, tools);
      expect(result).toMatchObject({ success: true, dryRun: true, analysisMode: 'open_and_validate', executable: 'UnrealInsights', arguments: ['-OpenTraceFile=<trace>', '-AutoQuit', '-NoUI'] });
    } finally {
      await fs.rm(tempRoot, { recursive: true, force: true });
    }
  });
});
