import { describe, expect, it, vi } from 'vitest';
import type { AutomationBridge } from '../../automation/index.js';
import type { ITools } from '../../types/tool-interfaces.js';
import { consolidatedToolDefinitions } from '../consolidated-tool-definitions.js';
import { GEOMETRY_ACTIONS, handleGeometryTools } from './geometry-handlers.js';

type SendAutomationRequest = (
  action: string,
  payload: Record<string, unknown>,
  options?: { timeoutMs?: number }
) => Promise<{ success: boolean }>;

function createConnectedTools() {
  const sendAutomationRequest = vi.fn<SendAutomationRequest>(async () => ({ success: true }));
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: vi.fn(async () => ({ success: true })),
      getProjectSettings: vi.fn(async () => ({}))
    },
    assetResources: {
      list: vi.fn(async () => ({}))
    },
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest
    } as unknown as AutomationBridge
  };

  return { tools, sendAutomationRequest };
}

describe('handleGeometryTools argument normalization', () => {
  it('normalizes non-finite vector and dimension components before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleGeometryTools('create_box', {
      action: 'create_box',
      name: 'GeoBox',
      dimensions: [100, Number.POSITIVE_INFINITY, '25', Number.NaN],
      location: { x: Number.NEGATIVE_INFINITY, y: '5', z: -3 }
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_geometry', expect.objectContaining({
      subAction: 'create_box',
      dimensions: [100, 0, 25, 0],
      location: [0, 5, -3]
    }), {});
  });

  it('drops non-finite UV aliases while preserving finite values', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleGeometryTools('transform_uvs', {
      action: 'transform_uvs',
      actorName: 'GeoBox',
      uvScale: { u: Number.POSITIVE_INFINITY, v: 2 },
      uvOffset: { u: 0.25, v: Number.NaN }
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_geometry', expect.objectContaining({
      subAction: 'transform_uvs',
      scaleV: 2,
      translateU: 0.25
    }), {});
    const payload = sendAutomationRequest.mock.calls[0]?.[1] ?? {};
    expect(payload).not.toHaveProperty('scaleU');
    expect(payload).not.toHaveProperty('translateV');
  });

  it('normalizes geometry creation path aliases before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleGeometryTools('create_box', {
      action: 'create_box',
      name: 'GeoBox',
      path: 'Game/MCPTest/Geometry'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_geometry', expect.objectContaining({
      subAction: 'create_box',
      path: '/Game/MCPTest/Geometry'
    }), {});
  });

  it('normalizes texture path aliases before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleGeometryTools('displace_by_texture', {
      action: 'displace_by_texture',
      actorName: 'GeoBox',
      texturePath: 'Content/MCPTest/Geometry/T_Displace'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_geometry', expect.objectContaining({
      subAction: 'displace_by_texture',
      texturePath: '/Game/MCPTest/Geometry/T_Displace'
    }), {});
  });

  it('normalizes outputPath aliases after mapping them to assetPath', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleGeometryTools('convert_to_static_mesh', {
      action: 'convert_to_static_mesh',
      actorName: 'GeoBox',
      outputPath: 'Content/GeneratedMeshes/SM_GeoBox'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_geometry', expect.objectContaining({
      subAction: 'convert_to_static_mesh',
      outputPath: '/Game/GeneratedMeshes/SM_GeoBox',
      assetPath: '/Game/GeneratedMeshes/SM_GeoBox'
    }), {});
  });

  it('normalizes LOD asset paths before Unreal dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleGeometryTools('set_lod_settings', {
      action: 'set_lod_settings',
      assetPath: 'Game/MCPTest/TestMesh',
      lodIndex: 0,
      reductionPercent: 50
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_geometry', expect.objectContaining({
      subAction: 'set_lod_settings',
      assetPath: '/Game/MCPTest/TestMesh',
      trianglePercent: 50
    }), {});
  });
});

describe('handleGeometryTools capability and advanced-action contracts', () => {
  it('keeps the public schema and TypeScript dispatch allow-list in lockstep', () => {
    const definition = consolidatedToolDefinitions.find((tool) => tool.name === 'manage_geometry');
    const properties = definition?.inputSchema.properties as Record<string, unknown> | undefined;
    const actionSchema = properties?.action as { enum?: unknown } | undefined;

    expect([...(actionSchema?.enum as string[])].sort()).toEqual([...GEOMETRY_ACTIONS].sort());
  });

  it('forwards capability inspection without requiring an actor or asset', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleGeometryTools('inspect_geometry_capabilities', {
      action: 'inspect_geometry_capabilities'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_geometry', {
      action: 'inspect_geometry_capabilities',
      subAction: 'inspect_geometry_capabilities'
    }, {});
  });

  it.each([
    'activate_modeling_tool', 'deactivate_modeling_tool', 'inspect_modeling_mode',
    'get_uv_set_bounds', 'get_num_uv_islands', 'unwrap_uvs', 'pack_uvs',
    'create_procedural_mesh', 'append_triangle', 'append_vertex',
    'delete_vertex', 'delete_triangle', 'get_vertex_position',
    'set_vertex_position', 'translate_mesh', 'set_uvs', 'set_vertex_color',
    'split_normals', 'difference'
  ])('forwards advertised geometry action %s', async (action) => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleGeometryTools(action, { action }, tools);

    expect(GEOMETRY_ACTIONS).toContain(action);
    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_geometry', expect.objectContaining({
      action,
      subAction: action
    }), {});
  });
});
