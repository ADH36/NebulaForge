#!/usr/bin/env node
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';

function usage() {
  console.log('Usage: node scripts/sync-vibeue-plugin.js (--source <VibeUE> | --repo <git-url>) --project <UnrealProject> [--ref <ref>] [--dry-run]');
  console.log('Copies the VibeUE plugin into <project>/Plugins/VibeUE after validating its manifest.');
}

function parseArgs(argv) {
  const result = { source: null, repo: null, ref: null, project: null, dryRun: false, help: false };
  for (let index = 2; index < argv.length; index += 1) {
    const token = argv[index];
    if (token === '--source' || token === '--repo' || token === '--project' || token === '--ref') {
      const value = argv[++index];
      if (!value || value.startsWith('--')) throw new Error(`Missing value for ${token}`);
      result[token.slice(2)] = value;
    } else if (token === '--dry-run') {
      result.dryRun = true;
    } else if (token === '--help' || token === '-h') {
      result.help = true;
    } else {
      throw new Error(`Unknown argument: ${token}`);
    }
  }
  return result;
}

function validateSource(source) {
  const root = path.resolve(source);
  const manifest = path.join(root, 'VibeUE.uplugin');
  if (!fs.existsSync(manifest)) throw new Error(`VibeUE.uplugin was not found under ${root}`);
  const parsed = JSON.parse(fs.readFileSync(manifest, 'utf8'));
  if (parsed.FriendlyName !== 'VibeUE' || !Array.isArray(parsed.Modules) || !parsed.Modules.some((module) => module.Name === 'VibeUE')) {
    throw new Error(`The source does not appear to be the VibeUE plugin: ${manifest}`);
  }
  return root;
}

function main() {
  const args = parseArgs(process.argv);
  if (args.help || (!args.source && !args.repo) || !args.project || (args.source && args.repo)) {
    usage();
    if (!args.help) process.exitCode = 1;
    return;
  }
  let temporaryClone;
  try {
    let source;
    if (args.repo) {
      if (!/^https:\/\/github\.com\/[^/]+\/[^/]+(?:\.git)?$/.test(args.repo)) {
        throw new Error('Only HTTPS GitHub repository URLs are accepted for --repo');
      }
      temporaryClone = fs.mkdtempSync(path.join(os.tmpdir(), 'nebula-vibeue-'));
      const cloneArgs = ['clone', '--depth', '1'];
      if (args.ref) cloneArgs.push('--branch', args.ref);
      cloneArgs.push(args.repo, temporaryClone);
      execFileSync('git', cloneArgs, { stdio: args.dryRun ? 'ignore' : 'pipe' });
      source = validateSource(temporaryClone);
    } else {
      source = validateSource(args.source);
    }
    const project = path.resolve(args.project);
    if (!fs.existsSync(project) || !fs.statSync(project).isDirectory()) throw new Error(`Unreal project directory does not exist: ${project}`);
    const destination = path.join(project, 'Plugins', 'VibeUE');
    if (args.dryRun) {
      console.log(`[dry-run] Would copy ${source} -> ${destination}`);
      return;
    }
    fs.mkdirSync(path.dirname(destination), { recursive: true });
    fs.cpSync(source, destination, { recursive: true, force: true });
    console.log(`Copied VibeUE to ${destination}`);
    console.log('Enable VibeUE, PythonScriptPlugin, and the UE 5.8 toolset plugins in the project, then rebuild the editor.');
  } finally {
    if (temporaryClone && fs.existsSync(temporaryClone)) fs.rmSync(temporaryClone, { recursive: true, force: true });
  }
}

try {
  main();
} catch (error) {
  console.error(`Error: ${error instanceof Error ? error.message : String(error)}`);
  process.exitCode = 1;
}
