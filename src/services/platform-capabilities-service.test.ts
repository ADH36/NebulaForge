import { describe, expect, it } from 'vitest';
import { inspectPlatformCapabilities } from './platform-capabilities-service.js';

describe('inspectPlatformCapabilities', () => {
  it('reports target platforms and signing-tool categories without mutating state', async () => {
    const result = await inspectPlatformCapabilities();
    expect(result.success).toBe(true);
    expect(result.targetPlatforms).toEqual(expect.arrayContaining(['Win64', 'Linux', 'Android', 'IOS']));
    expect(result.signingTools).toHaveProperty('android');
    expect(result.signingTools).toHaveProperty('win64');
  });
});
