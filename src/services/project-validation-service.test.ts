import { describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { runUnrealAutomationTests, validateProject } from './project-validation-service.js';

async function fixture(): Promise<string> {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-project-'));
  await fs.mkdir(path.join(root, 'Content'));
  await fs.mkdir(path.join(root, 'Config'));
  await fs.writeFile(path.join(root, 'Game.uproject'), JSON.stringify({ FileVersion: 3, Modules: [], Plugins: [] }));
  await fs.writeFile(path.join(root, 'Config', 'DefaultGame.ini'), '[/Script/EngineSettings.GeneralProjectSettings]\n');
  return root;
}

describe('validateProject', () => {
  it('validates a structurally sound Unreal project', async () => {
    const root = await fixture();
    const result = await validateProject({ projectPath: root, requiredFiles: ['Config/DefaultGame.ini'] });
    expect(result.success).toBe(true);
    expect(result.projectFile).toBe('Game.uproject');
    expect(result.inventory).toMatchObject({ assets: 0, maps: 0, configs: 1, truncated: false });
  });

  it('validates declared module layout and reports malformed module entries', async () => {
    const root = await fixture();
    await fs.mkdir(path.join(root, 'Source', 'Game'), { recursive: true });
    await fs.writeFile(path.join(root, 'Source', 'Game', 'Game.Build.cs'), 'using UnrealBuildTool;');
    await fs.writeFile(path.join(root, 'Game.uproject'), JSON.stringify({ FileVersion: 3, Modules: [{ Name: 'Game' }, {}], Plugins: [] }));
    const result = await validateProject({ projectPath: root });
    expect(result.success).toBe(false);
    expect(result.failedChecks).toContain('project_module_layout');
  });

  it('reports missing required project files and rejects traversal', async () => {
    const root = await fixture();
    const result = await validateProject({ projectPath: root, requiredFiles: ['../outside.txt', 'Config/Missing.ini'] });
    expect(result.success).toBe(false);
    expect(result.failedChecks).toEqual(expect.arrayContaining(['required_file']));
  });

  it('does not claim live validation started when UnrealEditor-Cmd is unavailable', async () => {
    const root = await fixture();
    const result = await validateProject({
      projectPath: root,
      validationMode: 'data_validation',
      enginePath: path.join(root, 'MissingEngine')
    });
    expect(result.success).toBe(false);
    expect(result.error).toBe('UNREAL_EDITOR_CMD_NOT_FOUND');
    expect(result.validation).toMatchObject({ mode: 'data_validation', started: false });
  });

  it('rejects commandlet argument injection and override tokens', async () => {
    const root = await fixture();
    const result = await validateProject({
      projectPath: root,
      validationMode: 'data_validation',
      enginePath: path.join(root, 'MissingEngine'),
      validationArguments: ['-project=C:/outside']
    });
    expect(result.success).toBe(false);
    expect(result.error).toBe('INVALID_ARGUMENT');
  });

  it('reports a managed automation-test launch boundary when UnrealEditor-Cmd is unavailable', async () => {
    const root = await fixture();
    const result = await runUnrealAutomationTests({
      projectPath: root,
      enginePath: path.join(root, 'MissingEngine'),
      filter: 'Game.Functional.*'
    });
    expect(result.success).toBe(false);
    expect(result.error).toBe('UNREAL_EDITOR_CMD_NOT_FOUND');
  });

  it('rejects unsafe automation test filters before process creation', async () => {
    const root = await fixture();
    const result = await runUnrealAutomationTests({
      projectPath: root,
      enginePath: path.join(root, 'MissingEngine'),
      filter: 'Game.Tests;Quit'
    });
    expect(result.success).toBe(false);
    expect(result.error).toBe('INVALID_ARGUMENT');
  });

  it('rejects automation report traversal and non-JSON paths', async () => {
    const root = await fixture();
    const result = await runUnrealAutomationTests({
      projectPath: root,
      enginePath: path.join(root, 'MissingEngine'),
      reportPath: '../outside.txt'
    });
    expect(result.success).toBe(false);
    expect(result.error).toBe('INVALID_REPORT_PATH');
  });
});
