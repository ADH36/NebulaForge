import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { afterEach, describe, expect, it } from 'vitest';
import { addLocalizationEntry, createLocalizationManifest, validateLocalizationManifest } from './localization-service.js';

const temporaryRoots: string[] = [];

afterEach(async () => {
  await Promise.all(temporaryRoots.splice(0).map(root => fs.rm(root, { recursive: true, force: true })));
});

async function makeProject(): Promise<string> {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'nebula-localization-'));
  await fs.mkdir(path.join(root, 'Content'), { recursive: true });
  temporaryRoots.push(root);
  return root;
}

describe('localization service', () => {
  it('creates, updates, and validates a localization manifest', async () => {
    const projectPath = await makeProject();
    const manifestPath = 'Content/Localization/Game.json';
    await createLocalizationManifest({
      projectPath,
      manifestPath,
      targetName: 'Game',
      sourceCulture: 'en',
      cultures: ['en', 'fr'],
      entries: []
    });
    const added = await addLocalizationEntry({
      projectPath,
      manifestPath,
      entry: { key: 'UI.Greeting', sourceText: 'Hello', translations: { fr: 'Bonjour' } }
    });
    expect(added).toMatchObject({ success: true, key: 'UI.Greeting', replaced: false, entryCount: 1 });
    await expect(validateLocalizationManifest({ projectPath, manifestPath, requireTranslations: true }))
      .resolves.toMatchObject({ success: true, valid: true, entryCount: 1, missingTranslations: [] });
  });

  it('reports missing required translations and rejects unsafe manifest paths', async () => {
    const projectPath = await makeProject();
    const manifestPath = 'Content/Localization/Game.json';
    await createLocalizationManifest({ projectPath, manifestPath, targetName: 'Game', sourceCulture: 'en', cultures: ['en', 'de'] });
    await addLocalizationEntry({ projectPath, manifestPath, entry: { key: 'UI.Start', sourceText: 'Start' } });
    const result = await validateLocalizationManifest({ projectPath, manifestPath, requireTranslations: true });
    expect(result).toMatchObject({ success: false, valid: false, missingTranslations: [{ key: 'UI.Start', culture: 'de' }] });
    await expect(createLocalizationManifest({ projectPath, manifestPath: 'Content/Other.json', targetName: 'Game', sourceCulture: 'en', cultures: ['en'] }))
      .rejects.toThrow('manifestPath must be under Content/Localization or Config/Localization');
  });
});
