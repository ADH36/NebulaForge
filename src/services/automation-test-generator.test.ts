import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { describe, expect, it } from 'vitest';
import { generateAutomationTest } from './automation-test-generator.js';

describe('generateAutomationTest', () => {
  it('writes a compile-ready automation test and header inside the project', async () => {
    const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-automation-test-'));
    try {
      const result = await generateAutomationTest({
        projectPath: root,
        className: 'FInventoryAutomationTest',
        testName: 'Game.Inventory.Smoke',
        headerPath: 'Source/Tests/InventoryAutomationTest.h',
        sourcePath: 'Source/Tests/InventoryAutomationTest.cpp'
      });
      const source = await fs.readFile(path.join(root, 'Source/Tests/InventoryAutomationTest.cpp'), 'utf8');
      expect(result).toMatchObject({ success: true, className: 'FInventoryAutomationTest', testName: 'Game.Inventory.Smoke' });
      expect(source).toContain('IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryAutomationTest, "Game.Inventory.Smoke"');
      expect(source).toContain('return !HasAnyErrors();');
    } finally {
      await fs.rm(root, { recursive: true, force: true });
    }
  });

  it('rejects unsafe identifiers, test names, and duplicate paths', async () => {
    await expect(generateAutomationTest({
      projectPath: os.tmpdir(),
      className: 'AutomationTest',
      testName: 'Game.Inventory.Smoke',
      headerPath: 'Source/Test.h',
      sourcePath: 'Source/Test.cpp'
    })).rejects.toThrow(/F-prefixed/);
    await expect(generateAutomationTest({
      projectPath: os.tmpdir(),
      className: 'FTest',
      testName: 'bad test',
      headerPath: 'Source/Test.h',
      sourcePath: 'Source/Test.cpp'
    })).rejects.toThrow(/testName/);
    await expect(generateAutomationTest({
      projectPath: os.tmpdir(),
      className: 'FTest',
      testName: 'Game.Test.Smoke',
      headerPath: 'Source/Test.h',
      sourcePath: 'Source/Test.h'
    })).rejects.toThrow(/different files/);
  });
});
