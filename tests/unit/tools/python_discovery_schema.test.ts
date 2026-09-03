import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../../src/tools/consolidated-tool-definitions.js';

describe('NebulaForge Python discovery actions', () => {
  it('exposes execution and discovery actions through system_control', () => {
    const definition = consolidatedToolDefinitions.find((tool) => tool.name === 'system_control');
    const actionSchema = definition?.inputSchema.properties?.action as { enum?: string[] } | undefined;

    expect(actionSchema?.enum).toEqual(expect.arrayContaining([
      'execute_python_code',
      'discover_python_module',
      'discover_python_class',
      'discover_python_function',
      'list_python_subsystems',
    ]));
  });

  it('defines discovery parameters alongside the existing Python boundary', () => {
    const definition = consolidatedToolDefinitions.find((tool) => tool.name === 'system_control');
    const properties = definition?.inputSchema.properties as Record<string, unknown> | undefined;

    expect(properties).toHaveProperty('moduleName');
    expect(properties).toHaveProperty('className');
    expect(properties).toHaveProperty('functionName');
    expect(properties).toHaveProperty('methodFilter');
  });

  it('exposes NebulaForge performance service methods as first-class actions', () => {
    const definition = consolidatedToolDefinitions.find((tool) => tool.name === 'system_control');
    const actionSchema = definition?.inputSchema.properties?.action as { enum?: string[] } | undefined;

    expect(actionSchema?.enum).toEqual(expect.arrayContaining([
      'frame_timing', 'force_hitch', 'performance_report', 'region_start', 'region_end',
      'start_standalone', 'stop_standalone', 'start_pie', 'stop_pie'
    ]));
  });
});
