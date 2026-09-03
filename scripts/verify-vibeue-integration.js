#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';

const requiredPlugins = ['PythonScriptPlugin', 'EditorScriptingUtilities', 'EnhancedInput', 'Niagara', 'ToolsetRegistry', 'ModelContextProtocol'];
const expectedSkillPacks = 38;

function usage() {
  console.log('Usage: node scripts/verify-vibeue-integration.js --project <UnrealProject>');
}

function projectArgument(argv) {
  const index = argv.indexOf('--project');
  if (index < 0 || !argv[index + 1] || argv[index + 1].startsWith('--')) return null;
  return path.resolve(argv[index + 1]);
}

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function main() {
  const project = projectArgument(process.argv);
  if (!project) {
    usage();
    process.exitCode = 1;
    return;
  }
  const vibeRoot = path.join(project, 'Plugins', 'VibeUE');
  const nebulaRoot = path.join(project, 'Plugins', 'NebulaForgeBridge');
  const vibeManifestPath = path.join(vibeRoot, 'VibeUE.uplugin');
  const nebulaManifestPath = path.join(nebulaRoot, 'NebulaForgeBridge.uplugin');
  if (!fs.existsSync(vibeManifestPath)) throw new Error(`VibeUE is not installed: ${vibeManifestPath}`);
  if (!fs.existsSync(nebulaManifestPath)) throw new Error(`NebulaForgeBridge is not installed: ${nebulaManifestPath}`);
  const vibeManifest = readJson(vibeManifestPath);
  const projectFiles = fs.readdirSync(project).filter((name) => name.endsWith('.uproject'));
  const projectConfig = projectFiles.length === 1 ? readJson(path.join(project, projectFiles[0])) : null;
  const enabled = new Set((projectConfig?.Plugins ?? []).filter((plugin) => plugin.Enabled !== false).map((plugin) => plugin.Name));
  const missing = requiredPlugins.filter((name) => !enabled.has(name));
  const skillsRoot = path.join(vibeRoot, 'Content', 'Skills');
  const skillCount = fs.existsSync(skillsRoot)
    ? fs.readdirSync(skillsRoot, { withFileTypes: true }).filter((entry) => entry.isDirectory() && fs.existsSync(path.join(skillsRoot, entry.name, 'SKILL.md'))).length
    : 0;
  console.log(JSON.stringify({
    project,
    vibeueVersion: vibeManifest.VersionName ?? vibeManifest.Version,
    vibeuePlugin: true,
    nebulaForgeBridge: true,
    skillPacks: skillCount,
    requiredPluginsMissingFromProject: projectConfig ? missing : [],
    skillPacksMissing: Math.max(0, expectedSkillPacks - skillCount),
    projectFileDetected: Boolean(projectConfig),
    readyForEditorBuild: Boolean(projectConfig) && missing.length === 0 && skillCount >= expectedSkillPacks
  }, null, 2));
  if (!projectConfig) process.exitCode = 2;
  else if (missing.length > 0 || skillCount < expectedSkillPacks) process.exitCode = 3;
}

try {
  main();
} catch (error) {
  console.error(`Error: ${error instanceof Error ? error.message : String(error)}`);
  process.exitCode = 1;
}
