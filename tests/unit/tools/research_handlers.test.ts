import { describe, expect, it, vi } from 'vitest';
import { handleResearchTools } from '../../../src/tools/handlers/research-handlers.js';

describe('deep_research', () => {
  it('rejects unsupported actions without network access', async () => {
    await expect(handleResearchTools({ action: 'nope' })).resolves.toMatchObject({
      success: false,
      error: 'UNKNOWN_ACTION'
    });
  });

  it('rejects unsafe fetch URLs', async () => {
    await expect(handleResearchTools({ action: 'fetch_page', url: 'file:///tmp/a' })).resolves.toMatchObject({
      success: false,
      error: 'RESEARCH_FAILED'
    });
  });

  it('normalizes reverse geocode longitude to lng', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      status: 200,
      text: async () => JSON.stringify({ display_name: 'Test', lat: '1', lon: '2' })
    }));
    await expect(handleResearchTools({ action: 'reverse_geocode', lat: '1', lng: '2' })).resolves.toMatchObject({
      success: true,
      lng: '2'
    });
    vi.unstubAllGlobals();
  });
});
