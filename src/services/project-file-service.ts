import { randomUUID } from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';

const MAX_WRITE_BYTES = 1 * 1024 * 1024;
const MAX_READ_BYTES = 2 * 1024 * 1024;
const PROTECTED_SEGMENTS = new Set(['.git', 'binaries', 'intermediate', 'saved', 'deriveddatacache', 'node_modules']);

function projectRootFromPath(projectPath?: string): string {
  const configured = projectPath || process.env.UE_PROJECT_PATH;
  if (!configured) throw new Error('UE_PROJECT_PATH or projectPath is required.');
  return configured.toLowerCase().endsWith('.uproject') ? path.dirname(configured) : configured;
}

function assertSafeRelativePath(filePath: string): void {
  if (!filePath || path.isAbsolute(filePath) || filePath.includes('\0')) {
    throw new Error('filePath must be a non-empty relative project path.');
  }
  const segments = filePath.replace(/\\/g, '/').split('/');
  if (segments.some(segment => segment === '..' || segment === '')) {
    throw new Error('filePath cannot contain parent-directory or empty path segments.');
  }
  if (segments.some(segment => PROTECTED_SEGMENTS.has(segment.toLowerCase()))) {
    throw new Error('filePath targets a protected project directory.');
  }
}

async function confinedPath(projectPath: string | undefined, filePath: string, allowMissing: boolean): Promise<{ root: string; target: string }> {
  assertSafeRelativePath(filePath);
  const root = await fs.realpath(projectRootFromPath(projectPath));
  const target = path.resolve(root, filePath);
  const relative = path.relative(root, target);
  if (relative.startsWith('..') || path.isAbsolute(relative)) throw new Error('filePath escapes the Unreal project root.');

  const parent = path.dirname(target);
  const resolvedParent = await fs.realpath(parent).catch(() => {
    if (!allowMissing) throw new Error(`Parent directory does not exist: ${parent}`);
    return undefined;
  });
  if (resolvedParent) {
    const parentRelative = path.relative(root, resolvedParent);
    if (parentRelative.startsWith('..') || path.isAbsolute(parentRelative)) throw new Error('filePath traverses outside the Unreal project root.');
  }
  if (!allowMissing) await fs.realpath(target);
  return { root, target };
}

export async function readProjectFile(projectPath: string | undefined, filePath: string): Promise<Record<string, unknown>> {
  const { root, target } = await confinedPath(projectPath, filePath, false);
  const stat = await fs.stat(target);
  if (!stat.isFile()) throw new Error('filePath is not a regular file.');
  if (stat.size > MAX_READ_BYTES) throw new Error(`Project file exceeds the ${MAX_READ_BYTES}-byte read limit.`);
  const content = await fs.readFile(target, 'utf8');
  return { success: true, projectRoot: root, filePath: path.relative(root, target).replace(/\\/g, '/'), content, bytes: Buffer.byteLength(content) };
}

export async function writeProjectFile(projectPath: string | undefined, filePath: string, content: string, backup: boolean = true): Promise<Record<string, unknown>> {
  if (typeof content !== 'string') throw new Error('content must be a string.');
  const size = Buffer.byteLength(content, 'utf8');
  if (size > MAX_WRITE_BYTES) throw new Error(`Project file exceeds the ${MAX_WRITE_BYTES}-byte write limit.`);
  const { root, target } = await confinedPath(projectPath, filePath, true);
  await fs.mkdir(path.dirname(target), { recursive: true });
  const exists = await fs.stat(target).then(stat => stat.isFile()).catch(() => false);
  let backupPath: string | undefined;
  if (exists && backup) {
    backupPath = `${target}.bak`;
    await fs.copyFile(target, backupPath);
  }
  const tempPath = `${target}.${randomUUID()}.tmp`;
  await fs.writeFile(tempPath, content, { encoding: 'utf8', mode: 0o600 });
  await fs.rename(tempPath, target);
  return {
    success: true,
    projectRoot: root,
    filePath: path.relative(root, target).replace(/\\/g, '/'),
    bytes: size,
    created: !exists,
    backupPath: backupPath ? path.relative(root, backupPath).replace(/\\/g, '/') : undefined
  };
}
