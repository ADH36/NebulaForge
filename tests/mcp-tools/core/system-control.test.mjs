#!/usr/bin/env node
/**
 * system_control Tool Integration Tests
 * Covers the core system-control actions, including Phase 34.6 subsystem and Phase 34.7
 * async/timer operations, and Phase 34.8 delegate/interface operations,
 * with proper setup/teardown sequencing.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/SystemControl';
const PHASE_348_FOLDER = '/Game/MCPTest/SystemControl348';
const PHASE_348_INTERFACE = `${PHASE_348_FOLDER}/BPI_SystemControl_348`;
const WIDGET_NAME = 'WBP_SystemControl_Test';
const WIDGET_PATH = `${TEST_FOLDER}/${WIDGET_NAME}`;
const VALIDATION_MATERIAL = `${TEST_FOLDER}/M_SystemControlValidation`;
const PYTHON_TEST_ID = Date.now();
const PYTHON_COMMAND_NAME = `McpPythonCommand_${PYTHON_TEST_ID}`;
const PYTHON_FILE_RELATIVE = `Saved/MCPTests/system-control-${PYTHON_TEST_ID}.py`;
const PYTHON_HELPER_RELATIVE = `Saved/MCPTests/system-control-${PYTHON_TEST_ID}-helper.txt`;
const PYTHON_FILE_LITERAL = JSON.stringify(PYTHON_FILE_RELATIVE);
const PYTHON_HELPER_LITERAL = JSON.stringify(PYTHON_HELPER_RELATIVE);
const PROJECT_SETTING_SECTION = '/Script/Engine.Engine';
const PROJECT_SETTING_KEY = `McpSystemControlSmoke_${Date.now()}`;
const PROJECT_SETTING_SECTION_LITERAL = JSON.stringify(PROJECT_SETTING_SECTION);
const PROJECT_SETTING_KEY_LITERAL = JSON.stringify(PROJECT_SETTING_KEY);
const CREATE_PYTHON_FILE_CODE = `
import os
import unreal
path = os.path.join(unreal.Paths.project_dir(), ${PYTHON_FILE_LITERAL})
helper_path = os.path.join(unreal.Paths.project_dir(), ${PYTHON_HELPER_LITERAL})
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(helper_path, 'w', encoding='utf-8') as f:
    f.write('sibling-file-ok')
with open(path, 'w', encoding='utf-8') as f:
    f.write('import os\\n')
    f.write('helper_path = os.path.join(os.path.dirname(__file__), os.path.basename(${PYTHON_HELPER_LITERAL}))\\n')
    f.write('with open(helper_path, "r", encoding="utf-8") as helper:\\n')
    f.write('    print("system-control-file-ok:" + helper.read())\\n')
print("system-control-file-created")
`.trim();
const DELETE_PYTHON_FILE_CODE = `
import os
import unreal
path = os.path.join(unreal.Paths.project_dir(), ${PYTHON_FILE_LITERAL})
helper_path = os.path.join(unreal.Paths.project_dir(), ${PYTHON_HELPER_LITERAL})
if os.path.exists(path):
    os.remove(path)
if os.path.exists(helper_path):
    os.remove(helper_path)
print("system-control-file-cleaned")
`.trim();
const CLEANUP_PROJECT_SETTING_CODE = `
import os
import unreal
section = ${PROJECT_SETTING_SECTION_LITERAL}
key = ${PROJECT_SETTING_KEY_LITERAL}
config_path = os.path.join(unreal.Paths.project_config_dir(), 'DefaultEngine.ini')
if os.path.exists(config_path):
    with open(config_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    kept = []
    in_target_section = False
    section_header = f'[{section}]'
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):
            in_target_section = stripped == section_header
        if in_target_section and stripped.split('=', 1)[0].strip() == key:
            continue
        kept.append(line)
    with open(config_path, 'w', encoding='utf-8') as f:
        f.writelines(kept)
print("system-control-setting-cleaned")
`.trim();

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create validation material', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_SystemControlValidation', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create Phase 34.8 folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: PHASE_348_FOLDER }, expected: 'success|already exists' },

  // === SUBSYSTEMS (PHASE 34.6) ===
  { scenario: 'INFO: list_subsystems', toolName: 'system_control', arguments: { action: 'list_subsystems', subsystemScope: 'world', subsystemName: 'WorldPartitionSubsystem' }, expected: 'success' },
  { scenario: 'CREATE: create_game_instance_subsystem', toolName: 'system_control', arguments: { action: 'create_game_instance_subsystem', subsystemClass: '/Script/Engine.GameInstanceSubsystem', worldContext: 'editor' }, expected: 'success|not initialized|not found|class not found' },
  { scenario: 'CREATE: create_world_subsystem', toolName: 'system_control', arguments: { action: 'create_world_subsystem', subsystemClass: '/Script/Engine.WorldPartitionSubsystem', worldContext: 'editor' }, expected: 'success|not initialized|not found|class not found' },
  { scenario: 'CREATE: create_local_player_subsystem', toolName: 'system_control', arguments: { action: 'create_local_player_subsystem', subsystemClass: '/Script/Engine.EnhancedInputLocalPlayerSubsystem', playerIndex: 0, worldContext: 'editor' }, expected: 'success|not initialized|not found|class not found' },
  { scenario: 'CREATE: create_engine_subsystem', toolName: 'system_control', arguments: { action: 'create_engine_subsystem', subsystemClass: '/Script/Engine.InputDeviceSubsystem' }, expected: 'success|not initialized|not found|class not found' },
  { scenario: 'INFO: get_subsystem', toolName: 'system_control', arguments: { action: 'get_subsystem', subsystemClass: '/Script/Engine.WorldPartitionSubsystem', subsystemScope: 'world', worldContext: 'editor' }, expected: 'success|not initialized|not found|class not found' },
  { scenario: 'INFO: inspect_subsystem', toolName: 'system_control', arguments: { action: 'inspect_subsystem', subsystemName: 'WorldPartitionSubsystem', subsystemScope: 'world', worldContext: 'editor' }, expected: 'success|not initialized|not found|class not found' },
  { scenario: 'CONFIG: configure_subsystem_tick', toolName: 'system_control', arguments: { action: 'configure_subsystem_tick', subsystemClass: '/Script/Engine.WorldPartitionSubsystem', subsystemScope: 'world', worldContext: 'editor', tickType: 'never', tickEnabled: false }, expected: 'success|not initialized|not found|class not found|tick not supported' },

  // === ASYNC & TIMERS (PHASE 34.7) ===
  { scenario: 'TIMER: set_timer', toolName: 'system_control', arguments: { action: 'set_timer', timerId: 'system-control-timer', rate: 60, firstDelay: 60, looping: false, worldContext: 'editor' }, expected: 'success|world context is unavailable' },
  { scenario: 'TIMER: get_timer', toolName: 'system_control', arguments: { action: 'get_timer', timerId: 'system-control-timer' }, expected: 'success|not found|world is no longer valid' },
  { scenario: 'TIMER: pause_timer', toolName: 'system_control', arguments: { action: 'pause_timer', timerId: 'system-control-timer' }, expected: 'success|not found|world is no longer valid' },
  { scenario: 'TIMER: resume_timer', toolName: 'system_control', arguments: { action: 'resume_timer', timerId: 'system-control-timer' }, expected: 'success|not found|world is no longer valid' },
  { scenario: 'INFO: list_timers', toolName: 'system_control', arguments: { action: 'list_timers' }, expected: 'success' },
  { scenario: 'TIMER: clear_timer', toolName: 'system_control', arguments: { action: 'clear_timer', timerId: 'system-control-timer' }, expected: 'success|not found|world is no longer valid' },
  { scenario: 'TIMER: callback validation', toolName: 'system_control', arguments: { action: 'set_timer', timerId: 'system-control-callback-timer', rate: 1, callbackObject: '/Game/MCPTest/MissingObject', callbackFunction: 'OnTimer' }, expected: 'invalid|callback|not found' },
  { scenario: 'LATENT: create_latent_action', toolName: 'system_control', arguments: { action: 'create_latent_action', latentId: 'system-control-latent', duration: 0, worldContext: 'editor' }, expected: 'success|world context is unavailable' },
  { scenario: 'INFO: get_latent_action', toolName: 'system_control', arguments: { action: 'get_latent_action', latentId: 'system-control-latent' }, expected: 'success|not found' },
  { scenario: 'INFO: list_latent_actions', toolName: 'system_control', arguments: { action: 'list_latent_actions' }, expected: 'success' },
  { scenario: 'LATENT: clear_latent_action', toolName: 'system_control', arguments: { action: 'clear_latent_action', latentId: 'system-control-latent' }, expected: 'success|not found' },
  { scenario: 'ASYNC: create_async_action', toolName: 'system_control', arguments: { action: 'create_async_action', asyncId: 'system-control-async', duration: 0.1, execution: 'thread_pool', label: 'system-control-test' }, expected: 'success' },
  { scenario: 'INFO: get_async_action', toolName: 'system_control', arguments: { action: 'get_async_action', asyncId: 'system-control-async' }, expected: 'success|not found' },
  { scenario: 'ASYNC: cancel_async_action', toolName: 'system_control', arguments: { action: 'cancel_async_action', asyncId: 'system-control-async' }, expected: 'success|not found' },
  { scenario: 'INFO: list_async_actions', toolName: 'system_control', arguments: { action: 'list_async_actions' }, expected: 'success' },
  { scenario: 'TASK: create_gameplay_task missing owner', toolName: 'system_control', arguments: { action: 'create_gameplay_task', taskId: 'system-control-task', ownerObject: '/Game/MCPTest/MissingOwner' }, expected: 'task owner not found|invalid|success' },
  { scenario: 'LATENT: callback and UUID validation', toolName: 'system_control', arguments: { action: 'create_latent_action', latentId: 'system-control-callback-latent', duration: 0, uuid: 19001, linkage: 2, callbackObject: '/Game/MCPTest/MissingObject', callbackFunction: 'OnLatentComplete' }, expected: 'invalid|callback|not found' },
  { scenario: 'TASK: optional task configuration', toolName: 'system_control', arguments: { action: 'create_gameplay_task', taskId: 'system-control-task-options', ownerObject: '/Game/MCPTest/MissingOwner', instanceName: 'SystemControlTask', priority: 10, activate: false, taskType: 'generic' }, expected: 'task owner not found|invalid|success' },
  { scenario: 'TASK: get_gameplay_task', toolName: 'system_control', arguments: { action: 'get_gameplay_task', taskId: 'system-control-task' }, expected: 'success|not found' },
  { scenario: 'INFO: list_gameplay_tasks', toolName: 'system_control', arguments: { action: 'list_gameplay_tasks' }, expected: 'success' },
  { scenario: 'TASK: end_gameplay_task', toolName: 'system_control', arguments: { action: 'end_gameplay_task', taskId: 'system-control-task' }, expected: 'success|not found' },
  { scenario: 'CONFIG: configure_task_priority', toolName: 'system_control', arguments: { action: 'configure_task_priority', taskId: 'system-control-task', priority: 10 }, expected: 'success|not found' },

  // === DELEGATES & INTERFACES (PHASE 34.8) ===
  { scenario: 'INTERFACE: create_blueprint_interface', toolName: 'system_control', arguments: { action: 'create_blueprint_interface', name: 'BPI_SystemControl_348', folder: PHASE_348_FOLDER }, expected: 'success|already exists' },
  { scenario: 'INTERFACE: add_interface_function', toolName: 'system_control', arguments: { action: 'add_interface_function', blueprintPath: PHASE_348_INTERFACE, interfaceFunction: 'OnSystemControlEvent', saveAsset: true }, expected: 'success|already exists|not found|invalid' },
  { scenario: 'DELEGATE: create_event_dispatcher', toolName: 'system_control', arguments: { action: 'create_event_dispatcher', blueprintPath: '/Game/MCPTest/MissingBlueprint', delegateName: 'OnSystemControlEvent', delegateKind: 'event_dispatcher', saveAsset: true }, expected: 'not found|invalid' },
  { scenario: 'DELEGATE: create_delegate', toolName: 'system_control', arguments: { action: 'create_delegate', blueprintPath: '/Game/MCPTest/MissingBlueprint', delegateName: 'SystemControlDelegate', delegateKind: 'single', saveAsset: false }, expected: 'not found|invalid' },
  { scenario: 'DELEGATE: bind_to_event', toolName: 'system_control', arguments: { action: 'bind_to_event', delegateObject: '/Game/MCPTest/MissingObject', delegateName: 'OnSystemControlEvent', targetObject: '/Game/MCPTest/MissingTarget', callbackFunction: 'OnSystemControlEvent' }, expected: 'not found|invalid' },
  { scenario: 'DELEGATE: bind_delegate with functionName', toolName: 'system_control', arguments: { action: 'bind_delegate', delegateObject: '/Game/MCPTest/MissingObject', delegateName: 'SystemControlDelegate', targetObject: '/Game/MCPTest/MissingTarget', functionName: 'HandleSystemControl' }, expected: 'not found|invalid' },
  { scenario: 'DELEGATE: unbind_from_event', toolName: 'system_control', arguments: { action: 'unbind_from_event', delegateObject: '/Game/MCPTest/MissingObject', delegateName: 'OnSystemControlEvent', targetObject: '/Game/MCPTest/MissingTarget', callbackFunction: 'OnSystemControlEvent' }, expected: 'not found|invalid' },
  { scenario: 'DELEGATE: broadcast_event', toolName: 'system_control', arguments: { action: 'broadcast_event', delegateObject: '/Game/MCPTest/MissingObject', delegateName: 'OnSystemControlEvent', parameterValues: { EventId: 'phase-34-8' } }, expected: 'not found|invalid' },
  { scenario: 'DELEGATE: inspect_delegate', toolName: 'system_control', arguments: { action: 'inspect_delegate', delegateObject: '/Game/MCPTest/MissingObject', delegateName: 'OnSystemControlEvent' }, expected: 'not found|invalid' },
  { scenario: 'DELEGATE: list_delegate_bindings', toolName: 'system_control', arguments: { action: 'list_delegate_bindings', delegateObject: '/Game/MCPTest/MissingObject', delegateName: 'OnSystemControlEvent' }, expected: 'not found|invalid' },
  { scenario: 'INTERFACE: implement_interface', toolName: 'system_control', arguments: { action: 'implement_interface', blueprintPath: '/Game/MCPTest/MissingBlueprint', interfacePath: PHASE_348_INTERFACE, interfaceClass: '/Script/Engine.MissingInterface', saveAsset: true }, expected: 'not found|invalid' },
  { scenario: 'INTERFACE: get_interface_info', toolName: 'system_control', arguments: { action: 'get_interface_info', targetObject: '/Game/MCPTest/MissingObject', interfaceClass: '/Script/Engine.MissingInterface' }, expected: 'not found|invalid' },
  { scenario: 'INTERFACE: call_interface_function', toolName: 'system_control', arguments: { action: 'call_interface_function', targetObject: '/Game/MCPTest/MissingObject', interfacePath: PHASE_348_INTERFACE, interfaceFunctionName: 'OnSystemControlEvent', parameterValues: {} }, expected: 'not found|invalid' },

  // === ACTION ===
  { scenario: 'ACTION: profile', toolName: 'system_control', arguments: { action: 'profile', profileType: 'cpu' }, expected: 'success' },
  { scenario: 'ACTION: show_fps', toolName: 'system_control', arguments: { action: 'show_fps', enabled: true }, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_quality', toolName: 'system_control', arguments: { action: 'set_quality', category: 'ViewDistance', level: 1 }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: screenshot', toolName: 'system_control', arguments: { action: 'screenshot', filename: 'SystemControl_NullRHI', resolution: '640x360', mode: 'editor_viewport', returnBase64: false, includeMetadata: true, metadata: { source: 'system-control-suite' } }, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_resolution', toolName: 'system_control', arguments: { action: 'set_resolution', width: 1280, height: 720, windowed: true }, expected: 'success' },
  { scenario: 'CONFIG: set_fullscreen', toolName: 'system_control', arguments: { action: 'set_fullscreen', enabled: false }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: execute_command', toolName: 'system_control', arguments: { action: 'execute_command', command: 'stat unit' }, expected: 'success' },
  { scenario: 'ACTION: console_command', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat fps' }, expected: 'success' },
  { scenario: 'ACTION: run_ubt', toolName: 'system_control', arguments: { action: 'run_ubt', target: 'MCPtestEditor', platform: 'Linux', configuration: 'Development', arguments: '-NoHotReload' }, expected: 'success' },
  { scenario: 'ACTION: subscribe', toolName: 'system_control', arguments: { action: 'subscribe' }, expected: 'success' },
  { scenario: 'ACTION: unsubscribe', toolName: 'system_control', arguments: { action: 'unsubscribe' }, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: spawn_category', toolName: 'system_control', arguments: { action: 'spawn_category', categoryName: 'AI' }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: start_session', toolName: 'system_control', arguments: { action: 'start_session', channels: 'cpu' }, expected: 'success' },
  { scenario: 'ACTION: lumen_update_scene', toolName: 'system_control', arguments: { action: 'lumen_update_scene' }, expected: 'success' },
  // === PLAYBACK ===
  { scenario: 'PLAYBACK: play_sound', toolName: 'system_control', arguments: { action: 'play_sound', volume: 0 }, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_widget', toolName: 'system_control', arguments: { action: 'create_widget', name: WIDGET_NAME, savePath: TEST_FOLDER }, expected: 'success|already exists' },
  // === ACTION ===
  { scenario: 'ACTION: show_widget', toolName: 'system_control', arguments: { action: 'show_widget', widgetId: 'notification', message: 'System control smoke', duration: 0.1 }, expected: 'success' },
  // === ADD ===
  { scenario: 'ADD: add_widget_child', toolName: 'system_control', arguments: { action: 'add_widget_child', widgetPath: WIDGET_PATH, childClass: 'TextBlock', name: 'SystemControlText', text: 'System control child' }, expected: 'success' },
  { scenario: 'ADD: add_widget_child parentName', toolName: 'system_control', arguments: { action: 'add_widget_child', widgetPath: WIDGET_PATH, childClass: 'TextBlock', name: 'SystemControlNestedText', parentName: 'RootCanvas', text: 'System control nested child' }, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_cvar', toolName: 'system_control', arguments: { action: 'set_cvar', name: 'r.ScreenPercentage', value: '100' }, expected: 'success' },
  // === INFO ===
  { scenario: 'INFO: get_project_settings', toolName: 'system_control', arguments: { action: 'get_project_settings', section: '/Script/Engine.Engine' }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: validate_assets', toolName: 'system_control', arguments: { action: 'validate_assets', paths: [VALIDATION_MATERIAL] }, expected: 'success' },
  { scenario: 'ACTION: validate_assets assetPath', toolName: 'system_control', arguments: { action: 'validate_assets', assetPath: VALIDATION_MATERIAL }, expected: 'success' },
  { scenario: 'ACTION: validate_assets path recursive', toolName: 'system_control', arguments: { action: 'validate_assets', path: TEST_FOLDER, recursive: false }, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_project_setting', toolName: 'system_control', arguments: { action: 'set_project_setting', section: PROJECT_SETTING_SECTION, key: PROJECT_SETTING_KEY, value: '1' }, expected: 'success' },
  { scenario: 'ACTION: execute_python', toolName: 'system_control', arguments: { action: 'execute_python', code: 'print("system-control-ok")' }, expected: 'success' },
  { scenario: 'ACTION: execute_python_string alias', toolName: 'system_control', arguments: { action: 'execute_python_string', code: 'print("system-control-string-ok")' }, expected: 'success' },
  { scenario: 'ACTION: configure_python_paths', toolName: 'system_control', arguments: { action: 'configure_python_paths', pythonPaths: ['/Content/Python'] }, expected: 'success' },
  { scenario: 'ACTION: list_python_packages', toolName: 'system_control', arguments: { action: 'list_python_packages' }, expected: 'success' },
  { scenario: 'ACTION: create_editor_utility_widget', toolName: 'system_control', arguments: { action: 'create_editor_utility_widget', assetPath: `${TEST_FOLDER}/EUW_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: inspect_editor_utility widget', toolName: 'system_control', arguments: { action: 'inspect_editor_utility', assetPath: `${TEST_FOLDER}/EUW_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: create_editor_utility_blueprint', toolName: 'system_control', arguments: { action: 'create_editor_utility_blueprint', assetPath: `${TEST_FOLDER}/EUB_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: create_python_editor_utility alias', toolName: 'system_control', arguments: { action: 'create_python_editor_utility', assetPath: `${TEST_FOLDER}/EUPython_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: create_geometry_collection', toolName: 'system_control', arguments: { action: 'create_geometry_collection', assetPath: `${TEST_FOLDER}/GC_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: create_geometry_collection source', toolName: 'system_control', arguments: { action: 'create_geometry_collection', assetPath: `${TEST_FOLDER}/GC_Source_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: add_geometry_to_collection', toolName: 'system_control', arguments: { action: 'add_geometry_to_collection', assetPath: `${TEST_FOLDER}/GC_SystemControl_${PYTHON_TEST_ID}`, sourceAssetPath: `${TEST_FOLDER}/GC_Source_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: remove_geometry_from_collection', toolName: 'system_control', arguments: { action: 'remove_geometry_from_collection', assetPath: `${TEST_FOLDER}/GC_SystemControl_${PYTHON_TEST_ID}`, sourceAssetPath: `${TEST_FOLDER}/GC_Source_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: configure_geometry_collection', toolName: 'system_control', arguments: { action: 'configure_geometry_collection', assetPath: `${TEST_FOLDER}/GC_SystemControl_${PYTHON_TEST_ID}`, mass: 25, massAsDensity: false, enableClustering: true, maxClusterLevel: 2, damageThresholds: [10, 25] }, expected: 'success' },
  { scenario: 'ACTION: inspect_geometry_collection', toolName: 'system_control', arguments: { action: 'inspect_geometry_collection', assetPath: `${TEST_FOLDER}/GC_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: configure_geometry_collection_component validation', toolName: 'system_control', arguments: { action: 'configure_geometry_collection_component', actorName: 'MissingGeometryCollectionActor' }, expected: 'error' },
  { scenario: 'ACTION: create_variant_set', toolName: 'system_control', arguments: { action: 'create_variant_set', assetPath: `${TEST_FOLDER}/LVS_SystemControl_${PYTHON_TEST_ID}`, variantSetName: 'SystemControlVariants' }, expected: 'success|error' },
  { scenario: 'ACTION: add_variant', toolName: 'system_control', arguments: { action: 'add_variant', assetPath: `${TEST_FOLDER}/LVS_SystemControl_${PYTHON_TEST_ID}`, variantSetName: 'SystemControlVariants', variantName: 'SystemControlDefault' }, expected: 'success|error' },
  { scenario: 'ACTION: configure_variant_properties validation', toolName: 'system_control', arguments: { action: 'configure_variant_properties', assetPath: `${TEST_FOLDER}/LVS_SystemControl_${PYTHON_TEST_ID}`, variantSetName: 'SystemControlVariants', variantName: 'SystemControlDefault', actorName: 'MissingVariantActor', propertyPath: 'bHidden', variantPropertyType: 'bool', variantPropertyValue: 'true' }, expected: 'error' },
  { scenario: 'ACTION: set_variant_dependencies validation', toolName: 'system_control', arguments: { action: 'set_variant_dependencies', assetPath: `${TEST_FOLDER}/LVS_SystemControl_${PYTHON_TEST_ID}`, variantSetName: 'SystemControlVariants', variantName: 'SystemControlDefault', dependencyVariantSetName: 'MissingDependencySet', dependencyVariantName: 'MissingDependencyVariant', enabled: true }, expected: 'error' },
  { scenario: 'ACTION: activate_variant validation', toolName: 'system_control', arguments: { action: 'activate_variant', assetPath: `${TEST_FOLDER}/LVS_SystemControl_${PYTHON_TEST_ID}`, variantSetName: 'MissingVariantSet', variantName: 'MissingVariant' }, expected: 'error' },
  { scenario: 'ACTION: get_active_variants validation', toolName: 'system_control', arguments: { action: 'get_active_variants', assetPath: '/Game/MCPTest/MissingLevelVariantSets' }, expected: 'error' },
  { scenario: 'ACTION: capture_variant_thumbnail validation', toolName: 'system_control', arguments: { action: 'capture_variant_thumbnail', assetPath: `${TEST_FOLDER}/LVS_SystemControl_${PYTHON_TEST_ID}`, variantSetName: 'MissingVariantSet', variantName: 'MissingVariant' }, expected: 'error' },
  { scenario: 'ACTION: set_variant_thumbnail validation', toolName: 'system_control', arguments: { action: 'set_variant_thumbnail', assetPath: `${TEST_FOLDER}/LVS_SystemControl_${PYTHON_TEST_ID}`, variantSetName: 'MissingVariantSet', variantName: 'MissingVariant', thumbnailSource: 'file', thumbnailPath: 'Saved/MissingVariant.png' }, expected: 'error' },
  { scenario: 'ACTION: export_variant_configuration validation', toolName: 'system_control', arguments: { action: 'export_variant_configuration', assetPath: '/Game/MCPTest/MissingLevelVariantSets', variantExportPath: 'Saved/VariantExports/Missing.json' }, expected: 'error' },
  { scenario: 'ACTION: set_ui_scale', toolName: 'system_control', arguments: { action: 'set_ui_scale', uiScale: 1.05 }, expected: 'success' },
  { scenario: 'ACTION: get_ui_scale', toolName: 'system_control', arguments: { action: 'get_ui_scale' }, expected: 'success' },
  { scenario: 'ACTION: configure_screen_reader_support', toolName: 'system_control', arguments: { action: 'configure_screen_reader_support', enabled: true }, expected: 'success|not supported' },
  { scenario: 'ACTION: announce_accessible_string', toolName: 'system_control', arguments: { action: 'announce_accessible_string', announcement: 'NebulaForge accessibility test announcement' }, expected: 'success' },
  { scenario: 'ACTION: set_screen_reader_text validation', toolName: 'system_control', arguments: { action: 'set_screen_reader_text', widgetPath: '/Game/MCPTest/MissingWidget', accessibleText: 'Missing widget' }, expected: 'error' },
  { scenario: 'ACTION: register_python_command', toolName: 'system_control', arguments: { action: 'register_python_command', commandName: PYTHON_COMMAND_NAME, commandSet: 'NebulaForgeMCPTests', commandContext: 'LevelEditor', commandLabel: 'NebulaForge MCP Test', commandDescription: 'Temporary integration-test command', code: 'print("nebula-forge-python-command-ok")' }, expected: 'success' },
  { scenario: 'ACTION: unregister_python_command', toolName: 'system_control', arguments: { action: 'unregister_python_command', commandName: PYTHON_COMMAND_NAME, commandSet: 'NebulaForgeMCPTests', commandContext: 'LevelEditor' }, expected: 'success' },
  { scenario: 'ACTION: inspect_editor_utility blueprint', toolName: 'system_control', arguments: { action: 'inspect_editor_utility', assetPath: `${TEST_FOLDER}/EUB_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'ACTION: run_editor_utility', toolName: 'system_control', arguments: { action: 'run_editor_utility', assetPath: `${TEST_FOLDER}/EUB_SystemControl_${PYTHON_TEST_ID}` }, expected: 'success' },
  { scenario: 'Setup: create execute_python file', toolName: 'system_control', arguments: { action: 'execute_python', code: CREATE_PYTHON_FILE_CODE }, expected: 'success' },
  // This result is asserted on the returned MCP response, so it also catches
  // temp wrapper cleanup racing file execution before output/status are written.
  { scenario: 'ACTION: execute_python file', toolName: 'system_control', arguments: { action: 'execute_python', file: PYTHON_FILE_RELATIVE }, expected: 'success', assertions: [{ path: 'structuredContent.result.output', equals: 'system-control-file-ok:sibling-file-ok', label: 'python file has __file__ and synchronous output' }] },
  { scenario: 'ACTION: execute_python_file rejects missing file', toolName: 'system_control', arguments: { action: 'execute_python_file', file: '/Content/Python/mcp_missing_file.py' }, expected: 'error' },
  { scenario: 'ACTION: execute_python_file rejects non-Python file', toolName: 'system_control', arguments: { action: 'execute_python_file', file: '/Content/Python/mcp_script.txt' }, expected: 'error' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete execute_python file', toolName: 'system_control', arguments: { action: 'execute_python', code: DELETE_PYTHON_FILE_CODE }, expected: 'success' },
  { scenario: 'Cleanup: remove project setting', toolName: 'system_control', arguments: { action: 'execute_python', code: CLEANUP_PROJECT_SETTING_CODE }, expected: 'success' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete Phase 34.8 folder', toolName: 'manage_asset', arguments: { action: 'delete', path: PHASE_348_FOLDER, force: true }, expected: 'success|not found' },
  { scenario: 'ACTION: run_tests', toolName: 'system_control', arguments: { action: 'run_tests', filter: 'System.Core.Time.Comparison' }, expected: 'success' },
];

// === PERFORMANCE ACTIONS ===
{
  /**
   * system_control performance action integration tests
   * Covers all 20 actions with proper setup/teardown sequencing.
   */

  const ts = Date.now();
  const TEST_FOLDER = `/Game/MCPTest/UtilityAssets_${ts}`;
  const MERGE_PARENT_ACTOR = `ParentActor_${ts}`;
  const MERGE_CHILD_ACTOR = `ChildActor_${ts}`;
  const MERGED_ACTOR_ASSET = `${TEST_FOLDER}/SM_PerformanceMerged_${ts}`;
  const MERGED_ACTOR_PACKAGE_ASSET = `${TEST_FOLDER}/SM_PerformanceMergedPackage_${ts}`;

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: spawn merge parent actor', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube', actorName: MERGE_PARENT_ACTOR, location: { x: 0, y: 300, z: 120 } }, expected: 'success|already exists' },
    { scenario: 'Setup: spawn merge child actor', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube', actorName: MERGE_CHILD_ACTOR, location: { x: 160, y: 300, z: 120 } }, expected: 'success|already exists' },

    // === ACTION ===
    { scenario: 'ACTION: start_profiling', toolName: 'system_control', arguments: {"action": "start_profiling", "type": "CPU", "duration": 1}, expected: 'success' },
    // === PLAYBACK ===
    { scenario: 'PLAYBACK: stop_profiling', toolName: 'system_control', arguments: {"action": "stop_profiling"}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: run_benchmark', toolName: 'system_control', arguments: {"action": "run_benchmark", "duration": 1, "type": "CPU"}, expected: 'success' },
    { scenario: 'ACTION: show_fps', toolName: 'system_control', arguments: {"action": "show_fps"}, expected: 'success' },
    { scenario: 'ACTION: show_stats', toolName: 'system_control', arguments: {"action": "show_stats", "category": "Unit"}, expected: 'success' },
    { scenario: 'ACTION: generate_memory_report', toolName: 'system_control', arguments: {"action": "generate_memory_report", "detailed": true}, expected: 'success|already exists' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_scalability', toolName: 'system_control', arguments: {"action": "set_scalability", "level": 1, "category": "ViewDistance"}, expected: 'success' },
    { scenario: 'CONFIG: set_resolution_scale', toolName: 'system_control', arguments: {"action": "set_resolution_scale", "scale": 75}, expected: 'success' },
    { scenario: 'CONFIG: set_vsync', toolName: 'system_control', arguments: {"action": "set_vsync", "enabled": true}, expected: 'success' },
    { scenario: 'CONFIG: set_frame_rate_limit', toolName: 'system_control', arguments: {"action": "set_frame_rate_limit", "maxFPS": 60}, expected: 'success' },
    // === TOGGLE ===
    { scenario: 'TOGGLE: enable_gpu_timing', toolName: 'system_control', arguments: {"action": "enable_gpu_timing"}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_texture_streaming', toolName: 'system_control', arguments: {"action": "configure_texture_streaming", "poolSize": 128, "boostPlayerLocation": false}, expected: 'success' },
    { scenario: 'CONFIG: configure_console_variables', toolName: 'system_control', arguments: {"action": "configure_console_variables", "consoleVariables": [{"name": "r.VSync", "value": "0"}, {"name": "t.MaxFPS", "value": "60"}]}, expected: 'success|error' },
    { scenario: 'CONFIG: configure_lod', toolName: 'system_control', arguments: {"action": "configure_lod", "forceLOD": -1, "lodBias": 0}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: apply_baseline_settings', toolName: 'system_control', arguments: {"action": "apply_baseline_settings"}, expected: 'success' },
    { scenario: 'ACTION: optimize_draw_calls', toolName: 'system_control', arguments: {"action": "optimize_draw_calls", "enableInstancing": false, "enableBatching": true}, expected: 'success' },
    { scenario: 'ACTION: merge_actors', toolName: 'system_control', arguments: {"action": "merge_actors", "actors": [MERGE_PARENT_ACTOR, MERGE_CHILD_ACTOR], "replaceSourceActors": false, "mergeActors": true, "outputPath": MERGED_ACTOR_ASSET}, expected: 'success' },
    { scenario: 'ACTION: merge_actors via packageName', toolName: 'system_control', arguments: {"action": "merge_actors", "actors": [MERGE_PARENT_ACTOR, MERGE_CHILD_ACTOR], "replaceSourceActors": false, "packageName": MERGED_ACTOR_PACKAGE_ASSET}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_occlusion_culling', toolName: 'system_control', arguments: {"action": "configure_occlusion_culling"}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: optimize_shaders', toolName: 'system_control', arguments: {"action": "optimize_shaders"}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_nanite', toolName: 'system_control', arguments: {"action": "configure_nanite"}, expected: 'success' },
    { scenario: 'CONFIG: configure_world_partition', toolName: 'system_control', arguments: {"action": "configure_world_partition", "cellSize": 6400, "streamingDistance": 25600}, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete merged mesh asset', toolName: 'manage_asset', arguments: { action: 'delete', path: MERGED_ACTOR_ASSET, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete packageName merged mesh asset', toolName: 'manage_asset', arguments: { action: 'delete', path: MERGED_ACTOR_PACKAGE_ASSET, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete merge actors', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [MERGE_PARENT_ACTOR, MERGE_CHILD_ACTOR] }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('system-control', testCases);
