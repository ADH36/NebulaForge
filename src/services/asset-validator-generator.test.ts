import { afterEach, describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { generateAssetValidator } from './asset-validator-generator.js';

describe('generateAssetValidator', () => {
  let root = '';
  afterEach(async () => { if (root) await fs.rm(root, { recursive: true, force: true }); });

  it('writes a context-aware UEditorValidatorBase skeleton', async () => {
    root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-validator-'));
    await fs.mkdir(path.join(root, 'Content'), { recursive: true });
    const result = await generateAssetValidator({ projectPath: root, className: 'UContractAssetValidator', headerPath: 'Source/Validation/ContractAssetValidator.h', sourcePath: 'Source/Validation/ContractAssetValidator.cpp' });
    expect(result.success).toBe(true);
    const header = await fs.readFile(path.join(root, 'Source/Validation/ContractAssetValidator.h'), 'utf8');
    const source = await fs.readFile(path.join(root, 'Source/Validation/ContractAssetValidator.cpp'), 'utf8');
    expect(header).toContain('UEditorValidatorBase');
    expect(source).toContain('ValidateLoadedAsset_Implementation');
    expect(source).toContain('AssetPasses(InObject)');
  });

  it('rejects invalid class naming', async () => {
    await expect(generateAssetValidator({ className: 'ContractValidator', headerPath: 'Validator.h', sourcePath: 'Validator.cpp' })).rejects.toThrow(/U-prefixed/);
  });
});
