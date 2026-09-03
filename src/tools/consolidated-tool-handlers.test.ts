import { describe, expect, it, vi } from 'vitest';
import { mkdtemp, writeFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import type { AutomationBridge } from '../automation/index.js';
import type { ITools } from '../types/tool-interfaces.js';
import { handleConsolidatedToolCall } from './consolidated-tool-handlers.js';
import { consolidatedToolDefinitions } from './consolidated-tool-definitions.js';
import { coreToolDefinitions } from './schemas/core-tools.js';

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
  } as unknown as ITools;

  return { tools, sendAutomationRequest };
}

describe('consolidated action params compatibility', () => {
  it('exposes the five focused domain parent tools with their action groups', () => {
    const expectedActions: Record<string, string> = {
      manage_materials: 'create_material',
      manage_lighting: 'spawn_light',
      manage_input: 'create_input_action',
      manage_ui: 'create_widget_blueprint',
      manage_navigation: 'query_navigation_path'
    };

    for (const [toolName, actionName] of Object.entries(expectedActions)) {
      const tool = consolidatedToolDefinitions.find((definition) => definition.name === toolName);
      const inputSchema = tool?.inputSchema as { properties?: Record<string, unknown> } | undefined;
      const action = inputSchema?.properties?.action as { enum?: string[] } | undefined;

      expect(tool).toBeDefined();
      expect(action?.enum).toContain(actionName);
      expect(inputSchema?.properties).toHaveProperty('params');
    }
    expect(consolidatedToolDefinitions).toHaveLength(32);
  });

  it.each([
    ['manage_materials', 'create_material', 'manage_material_authoring', { name: 'MCP_Test', path: '/Game/Materials' }],
    ['manage_lighting', 'spawn_light', 'manage_lighting', { name: 'MCP_Test' }],
    ['manage_input', 'create_input_action', 'manage_input', { name: 'MCP_Test', path: '/Game/Input' }],
    ['manage_ui', 'create_widget_blueprint', 'manage_widget_authoring', { name: 'MCP_Test', folder: '/Game/UI' }],
    ['manage_navigation', 'query_navigation_path', 'manage_navigation', { start: { x: 0, y: 0, z: 0 }, end: { x: 100, y: 100, z: 0 } }]
  ])('routes %s directly to the focused domain handler', async (toolName, action, bridgeTool, domainArgs) => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall(toolName, { action, params: domainArgs }, tools);

    if (toolName === 'manage_lighting') {
      expect(sendAutomationRequest).toHaveBeenCalledWith(action, expect.any(Object), expect.any(Object));
    } else {
      expect(sendAutomationRequest).toHaveBeenCalledWith(bridgeTool, expect.objectContaining({
        subAction: action
      }), expect.any(Object));
    }
  });

  it('advertises pattern texture parameters on the consolidated asset schema', () => {
    const tool = consolidatedToolDefinitions.find((definition) => definition.name === 'manage_asset');
    const inputSchema = tool?.inputSchema as Record<string, unknown> | undefined;
    const properties = inputSchema?.properties as Record<string, unknown> | undefined;
    const action = properties?.action as { enum?: string[] } | undefined;

    expect(action?.enum).toContain('create_pattern_texture');
    for (const property of ['path', 'width', 'height', 'pattern', 'patternType', 'primaryColor', 'secondaryColor', 'tilesX', 'tilesY', 'lineWidth', 'brickRatio', 'offset']) {
      expect(properties).toHaveProperty(property);
    }
  });

  it('routes pattern texture aliases without leaking the consolidated action to strict texture validation', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_asset', {
      action: 'create_pattern_texture',
      name: 'MCP_Pattern',
      path: '/Game/Textures',
      pattern: 'Dots',
      width: 64,
      height: 32
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_texture', expect.objectContaining({
      subAction: 'create_pattern_texture',
      name: 'MCP_Pattern',
      path: '/Game/Textures',
      patternType: 'Dots',
      width: 64,
      height: 32
    }), expect.any(Object));
    expect(sendAutomationRequest.mock.calls[0]?.[1]).not.toHaveProperty('action');
  });

  it('advertises params for action tools in public schemas', () => {
    const tools = [
      consolidatedToolDefinitions.find((tool) => tool.name === 'manage_level_structure'),
      consolidatedToolDefinitions.find((tool) => tool.name === 'system_control'),
      coreToolDefinitions.find((tool) => tool.name === 'manage_level')
    ];

    for (const tool of tools) {
      const inputSchema = tool?.inputSchema as Record<string, unknown> | undefined;
      const properties = inputSchema?.properties as Record<string, unknown> | undefined;
      expect(properties).toHaveProperty('params');
      expect(inputSchema?.additionalProperties).toBe(true);
    }
  });

  it('merges params into level-structure payloads before validation and dispatch', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_level_structure', {
      action: 'create_level',
      params: {
        levelName: 'MCP_Racing_Level',
        levelPath: '/Game/MCP_Racing_Level',
        save: true
      }
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level_structure', expect.objectContaining({
      action: 'create_level',
      subAction: 'create_level',
      levelName: 'MCP_Racing_Level',
      levelPath: '/Game/MCP_Racing_Level',
      save: true
    }), expect.any(Object));
    const firstCall = sendAutomationRequest.mock.calls[0];
    expect(firstCall).toBeDefined();
    const payload = firstCall?.[1] ?? {};
    expect(payload).not.toHaveProperty('params');
  });

  it('lets top-level arguments override params when both are provided', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_level_structure', {
      action: 'create_level',
      levelName: 'TopLevelName',
      params: {
        action: 'create_sublevel',
        levelName: 'NestedName'
      }
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level_structure', expect.objectContaining({
      action: 'create_level',
      subAction: 'create_level',
      levelName: 'TopLevelName'
    }), expect.any(Object));
  });

  it('removes params before routing to strict input handlers', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_networking', {
      action: 'create_input_action',
      params: {
        name: 'IA_Throttle',
        path: '/Game/Input'
      }
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_input', expect.objectContaining({
      action: 'create_input_action',
      subAction: 'create_input_action',
      name: 'IA_Throttle',
      path: '/Game/Input'
    }), expect.any(Object));
    const firstCall = sendAutomationRequest.mock.calls[0];
    expect(firstCall).toBeDefined();
    const payload = firstCall?.[1] ?? {};
    expect(payload).not.toHaveProperty('params');
  });

  it('routes base audio asset creation through audio authoring', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_audio', {
      action: 'create_sound_mix',
      name: 'SCM_Test',
      path: '/Game/Audio'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_audio_authoring', expect.objectContaining({
      subAction: 'create_sound_mix',
      name: 'SCM_Test',
      path: '/Game/Audio'
    }), expect.any(Object));
  });

  it('preserves sound cue aliases when routing through audio authoring', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_audio', {
      action: 'create_sound_cue',
      name: 'SC_TestCue',
      soundPath: '/Engine/VREditor/Sounds/VR_click1',
      savePath: '/Game/Audio/Cues'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_audio_authoring', expect.objectContaining({
      subAction: 'create_sound_cue',
      name: 'SC_TestCue',
      path: '/Game/Audio/Cues',
      wavePath: '/Engine/VREditor/Sounds/VR_click1'
    }), expect.any(Object));
  });

  it('forwards overwrite for level copy-style actions', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('manage_level', {
      action: 'duplicate_level',
      sourcePath: '/Game/Maps/Source',
      destinationPath: '/Game/Maps/Destination',
      overwrite: true
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('manage_level', expect.objectContaining({
      action: 'duplicate',
      sourcePath: '/Game/Maps/Source',
      destinationPath: '/Game/Maps/Destination',
      overwrite: true
    }), expect.any(Object));
  });

  it('returns structured error context for unknown consolidated tools', async () => {
    const { tools } = createConnectedTools();

    const result = await handleConsolidatedToolCall('missing_tool', { action: 'probe' }, tools) as Record<string, unknown>;

    expect(result).toMatchObject({
      success: false,
      isError: true,
      error: 'UNKNOWN_TOOL',
      toolName: 'missing_tool',
      action: 'probe'
    });
    expect(String(result.message)).toContain('Unknown consolidated tool: missing_tool');
  });

  it('preserves tool and action context on dispatch exceptions', async () => {
    const { tools } = createConnectedTools();

    const result = await handleConsolidatedToolCall('manage_level_structure', {
      action: 'create_level',
      levelName: 'BadLevel',
      levelPath: '/etc/passwd'
    }, tools) as Record<string, unknown>;

    expect(result).toMatchObject({
      success: false,
      isError: true,
      error: 'SECURITY_VIOLATION',
      toolName: 'manage_level_structure',
      action: 'create_level'
    });
    expect(String(result.message)).toContain('Security violation');
  });

  it('routes full editor screenshot mode with base64 image return enabled', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('system_control', {
      action: 'screenshot',
      filename: 'FullEditor',
      mode: 'full_editor_window'
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_editor', {
      action: 'screenshot',
      filename: 'FullEditor',
      resolution: undefined,
      mode: 'full_editor_window',
      returnBase64: true
    }, {});
  });

  it('routes host-only deployment and soak actions to the pipeline handler', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();
    const root = await mkdtemp(join(tmpdir(), 'nebula-consolidated-pipeline-'));
    try {
      await writeFile(join(root, 'Game.apk'), 'apk');
      const result = await handleConsolidatedToolCall('system_control', {
        action: 'deploy_package',
        platform: 'Android',
        archiveDirectory: root,
        artifactPath: 'Game.apk',
        deviceId: 'emulator-5554',
        dryRun: true
      }, tools);

      expect(result).toMatchObject({
        success: true,
        dryRun: true,
        platform: 'Android',
        command: 'adb'
      });
      expect(sendAutomationRequest).not.toHaveBeenCalled();
    } finally {
      await rm(root, { recursive: true, force: true });
    }
  });

  it('forwards screenshot metadata opt-in for system control screenshots', async () => {
    const { tools, sendAutomationRequest } = createConnectedTools();

    await handleConsolidatedToolCall('system_control', {
      action: 'screenshot',
      filename: 'FullEditor',
      mode: 'full_editor_window',
      includeMetadata: true
    }, tools);

    expect(sendAutomationRequest).toHaveBeenCalledWith('control_editor', {
      action: 'screenshot',
      filename: 'FullEditor',
      resolution: undefined,
      mode: 'full_editor_window',
      returnBase64: true,
      includeMetadata: true
    }, {});
  });
});
