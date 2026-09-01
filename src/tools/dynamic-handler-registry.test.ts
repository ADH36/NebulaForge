import { describe, expect, it } from 'vitest';
import { DynamicHandlerRegistry } from './dynamic-handler-registry.js';

describe('DynamicHandlerRegistry', () => {
  it('registers and resolves normalized handler names', () => {
    const registry = new DynamicHandlerRegistry();
    const handler = async () => ({ success: true });
    registry.register('  custom_action  ', handler);

    expect(registry.hasHandler('custom_action')).toBe(true);
    expect(registry.getHandler('custom_action')).toBe(handler);
    expect(registry.getAllRegisteredTools()).toEqual(['custom_action']);
  });

  it('replaces an existing handler deterministically', () => {
    const registry = new DynamicHandlerRegistry();
    const first = async () => ({ value: 1 });
    const second = async () => ({ value: 2 });
    registry.register('custom_action', first);
    registry.register('custom_action', second);

    expect(registry.getHandler('custom_action')).toBe(second);
    expect(registry.removeHandler('custom_action')).toBe(true);
    expect(registry.hasHandler('custom_action')).toBe(false);
  });
});
