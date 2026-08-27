#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();

const MAIN_ACTOR = `MCP_CoreActor_${ts}`;
const DELETE_ACTOR = `MCP_DeleteActor_${ts}`;
const DESTROY_ACTOR = `MCP_DestroyActor_${ts}`;
const TAG_DELETE_ACTOR = `MCP_TagDeleteActor_${ts}`;
const DUPLICATE_ACTOR = `MCP_DuplicateActor_${ts}`;
const DUPLICATE_COPY = `MCP_DuplicateActorCopy_${ts}`;
const MESH_ACTOR = `MCP_MeshActor_${ts}`;
const PARENT_ACTOR = `MCP_ParentActor_${ts}`;
const CHILD_ACTOR = `MCP_ChildActor_${ts}`;
const BP_NAME = `BP_ControlActor_${ts}`;
const BP_PATH = `${TEST_FOLDER}/${BP_NAME}`;
const BP_ACTOR = `MCP_BlueprintActor_${ts}`;
const TAG = `MCPControlActorTag_${ts}`;
const DELETE_TAG = `MCPDeleteTag_${ts}`;
const COMPONENT_NAME = `MCPPointLight_${ts}`;
const ENGINE_BASIC_MATERIAL = '/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial';

const cubeSpawn = (scenario, actorName, location) => ({
  scenario,
  toolName: 'control_actor',
  arguments: {
    action: 'spawn',
    classPath: '/Engine/BasicShapes/Cube',
    actorName,
    location,
  },
  expected: 'success|already exists',
});

const actorArgs = (action, extra = {}) => ({ action, actorName: MAIN_ACTOR, ...extra });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create actor blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: BP_NAME, path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  cubeSpawn('Setup: spawn main test actor', MAIN_ACTOR, { x: 0, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn delete test actor', DELETE_ACTOR, { x: 120, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn destroy test actor', DESTROY_ACTOR, { x: 240, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn tag-delete test actor', TAG_DELETE_ACTOR, { x: 360, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn duplicate test actor', DUPLICATE_ACTOR, { x: 480, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn attach parent actor', PARENT_ACTOR, { x: 600, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn attach child actor', CHILD_ACTOR, { x: 720, y: 0, z: 100 }),
  { scenario: 'Setup: tag actor for delete_by_tag', toolName: 'control_actor', arguments: { action: 'add_tag', actorName: TAG_DELETE_ACTOR, tag: DELETE_TAG }, expected: 'success|already exists' },

  // === SPAWN / DELETE ===
  { scenario: 'ACTION: spawn', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: `MCP_SpawnSphere_${ts}`, location: { x: 0, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_actor', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Engine/BasicShapes/Cylinder', actorName: `MCP_SpawnCylinder_${ts}`, location: { x: 120, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_actor with meshPath', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube', actorName: MESH_ACTOR, location: { x: 360, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_blueprint', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: BP_PATH, actorName: BP_ACTOR, location: { x: 240, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'DELETE: delete', toolName: 'control_actor', arguments: { action: 'delete', actorName: DELETE_ACTOR }, expected: 'success|not found' },
  { scenario: 'DELETE: destroy_actor', toolName: 'control_actor', arguments: { action: 'destroy_actor', actorName: DESTROY_ACTOR }, expected: 'success|not found' },
  { scenario: 'DELETE: delete_by_tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: DELETE_TAG }, expected: 'success|not found' },

  // === TRANSFORM / PHYSICS ===
  { scenario: 'ACTION: duplicate', toolName: 'control_actor', arguments: { action: 'duplicate', actorName: DUPLICATE_ACTOR, newName: DUPLICATE_COPY, offset: { x: 50, y: 0, z: 0 } }, expected: 'success|already exists' },
  { scenario: 'CONFIG: set_transform', toolName: 'control_actor', arguments: actorArgs('set_transform', { location: { x: 10, y: 20, z: 130 }, rotation: { x: 0, y: 0, z: 15 }, scale: { x: 1.1, y: 1.1, z: 1.1 } }), expected: 'success' },
  { scenario: 'ACTION: teleport_actor', toolName: 'control_actor', arguments: actorArgs('teleport_actor', { location: { x: 20, y: 30, z: 140 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_location', toolName: 'control_actor', arguments: actorArgs('set_actor_location', { location: { x: 30, y: 40, z: 150 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_rotation', toolName: 'control_actor', arguments: actorArgs('set_actor_rotation', { rotation: { x: 0, y: 45, z: 0 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_scale', toolName: 'control_actor', arguments: actorArgs('set_actor_scale', { scale: { x: 1.25, y: 1.25, z: 1.25 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_transform', toolName: 'control_actor', arguments: actorArgs('set_actor_transform', { location: { x: 40, y: 50, z: 160 }, rotation: { x: 0, y: 0, z: 30 }, scale: { x: 1, y: 1, z: 1 } }), expected: 'success' },
  { scenario: 'INFO: get_transform', toolName: 'control_actor', arguments: actorArgs('get_transform'), expected: 'success' },
  { scenario: 'INFO: get_actor_transform', toolName: 'control_actor', arguments: actorArgs('get_actor_transform'), expected: 'success' },
  { scenario: 'CONFIG: set_visibility', toolName: 'control_actor', arguments: actorArgs('set_visibility', { visible: true }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_visible', toolName: 'control_actor', arguments: actorArgs('set_actor_visible', { visible: true }), expected: 'success' },
  { scenario: 'ACTION: apply_force', toolName: 'control_actor', arguments: actorArgs('apply_force', { force: { x: 0, y: 0, z: 2500 } }), expected: 'success' },
  { scenario: 'CONFIG: set_material', toolName: 'control_actor', arguments: actorArgs('set_material', { materialPath: ENGINE_BASIC_MATERIAL, materialSlot: 0 }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_material', toolName: 'control_actor', arguments: actorArgs('set_actor_material', { materialPath: ENGINE_BASIC_MATERIAL, materialIndex: 0 }), expected: 'success' },
  { scenario: 'CONFIG: apply_material all components', toolName: 'control_actor', arguments: actorArgs('apply_material', { materialPath: ENGINE_BASIC_MATERIAL, materialSlot: 0, allComponents: true }), expected: 'success' },

  // === COMPONENTS ===
  { scenario: 'ADD: add_component', toolName: 'control_actor', arguments: actorArgs('add_component', { componentType: '/Script/Engine.PointLightComponent', componentName: COMPONENT_NAME, properties: { Intensity: 1250 } }), expected: 'success|already exists' },
  { scenario: 'CONFIG: set_component_properties', toolName: 'control_actor', arguments: actorArgs('set_component_properties', { componentName: COMPONENT_NAME, properties: { Intensity: 1800 } }), expected: 'success' },
  { scenario: 'CONFIG: set_component_property', toolName: 'control_actor', arguments: actorArgs('set_component_property', { componentName: COMPONENT_NAME, propertyName: 'Intensity', value: 950 }), expected: 'success' },
  { scenario: 'INFO: get_component_property', toolName: 'control_actor', arguments: actorArgs('get_component_property', { componentName: COMPONENT_NAME, propertyName: 'Intensity' }), expected: 'success' },
  { scenario: 'INFO: get_components', toolName: 'control_actor', arguments: actorArgs('get_components'), expected: 'success' },
  { scenario: 'INFO: get_actor_components', toolName: 'control_actor', arguments: actorArgs('get_actor_components'), expected: 'success' },
  { scenario: 'INFO: get_actor_bounds', toolName: 'control_actor', arguments: actorArgs('get_actor_bounds'), expected: 'success' },
  { scenario: 'DELETE: remove_component', toolName: 'control_actor', arguments: actorArgs('remove_component', { componentName: COMPONENT_NAME }), expected: 'success|not found' },

  // === COLLISION PROFILES / CHANNELS (PHASE 34.4) ===
  { scenario: 'COLLISION: create object channel', toolName: 'control_actor', arguments: { action: 'create_collision_channel', channelName: `MCPObjectChannel_${ts}`, channelType: 'object', defaultResponse: 'block', staticObject: false, traceType: false, saveConfig: false, helpMessage: `MCP collision channel ${ts}` }, expected: 'success|already configured|no channel slot' },
  { scenario: 'COLLISION: create trace channel', toolName: 'control_actor', arguments: { action: 'create_collision_channel', channelName: `MCPTraceChannel_${ts}`, channelType: 'trace', defaultResponse: 'overlap', staticObject: false, traceType: true, saveConfig: false }, expected: 'success|already configured|no channel slot' },
  { scenario: 'COLLISION: create profile', toolName: 'control_actor', arguments: { action: 'create_collision_profile', profileName: `MCPProfile_${ts}`, collisionMode: 'query_and_physics', objectType: 'WorldDynamic', responses: { WorldStatic: 'block', Visibility: 'ignore', Camera: 'overlap' }, saveConfig: false, helpMessage: `MCP collision profile ${ts}` }, expected: 'success|already exists' },
  { scenario: 'COLLISION: validate profile', toolName: 'control_actor', arguments: { action: 'validate_collision_profile', profileName: `MCPProfile_${ts}`, saveConfig: false }, expected: 'success' },
  { scenario: 'COLLISION: configure object type', toolName: 'control_actor', arguments: actorArgs('configure_object_type', { objectType: 'WorldDynamic', collisionMode: 'query_and_physics' }), expected: 'success' },
  { scenario: 'COLLISION: configure trace channel', toolName: 'control_actor', arguments: actorArgs('configure_trace_channel', { traceChannel: 'Visibility', response: 'block', channelName: 'Visibility' }), expected: 'success' },
  { scenario: 'COLLISION: configure response batch', toolName: 'control_actor', arguments: actorArgs('configure_channel_responses', { responses: { WorldStatic: 'block', Pawn: 'overlap', Camera: 'ignore' } }), expected: 'success' },
  { scenario: 'COLLISION: apply actor profile', toolName: 'control_actor', arguments: actorArgs('set_actor_collision_profile', { profileName: `MCPProfile_${ts}` }), expected: 'success' },
  { scenario: 'COLLISION: apply component profile', toolName: 'control_actor', arguments: actorArgs('set_component_collision_profile', { componentName: 'StaticMeshComponent0', profileName: `MCPProfile_${ts}` }), expected: 'success|not found' },
  { scenario: 'COLLISION: read actor state', toolName: 'control_actor', arguments: actorArgs('get_actor_collision'), expected: 'success' },
  { scenario: 'COLLISION: read component state', toolName: 'control_actor', arguments: actorArgs('get_component_collision', { componentName: 'StaticMeshComponent0' }), expected: 'success|not found' },

  // === TAGS / SEARCH ===
  { scenario: 'ADD: add_tag', toolName: 'control_actor', arguments: actorArgs('add_tag', { tag: TAG }), expected: 'success|already exists' },
  { scenario: 'INFO: find_by_tag', toolName: 'control_actor', arguments: { action: 'find_by_tag', tag: TAG }, expected: 'success' },
  { scenario: 'INFO: find_actors_by_tag', toolName: 'control_actor', arguments: { action: 'find_actors_by_tag', tag: TAG }, expected: 'success' },
  { scenario: 'INFO: find_by_name', toolName: 'control_actor', arguments: { action: 'find_by_name', name: MAIN_ACTOR }, expected: 'success' },
  { scenario: 'INFO: find_actors_by_name', toolName: 'control_actor', arguments: { action: 'find_actors_by_name', name: MAIN_ACTOR }, expected: 'success' },
  { scenario: 'INFO: find_by_class', toolName: 'control_actor', arguments: { action: 'find_by_class', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'INFO: find_actors_by_class', toolName: 'control_actor', arguments: { action: 'find_actors_by_class', className: 'StaticMeshActor' }, expected: 'success' },
  // === SELECTION / GROUPING (PHASE 34.3) ===
  { scenario: 'SELECT: select_actor', toolName: 'control_actor', arguments: { action: 'select_actor', actorName: MAIN_ACTOR, replaceSelection: true, selectEvenIfHidden: true, warnIfLevelLocked: false }, expected: 'success' },
  { scenario: 'SELECT: select_actors_by_class', toolName: 'control_actor', arguments: { action: 'select_actors_by_class', classPath: '/Script/Engine.StaticMeshActor', replaceSelection: true, includeDerivedClasses: true }, expected: 'success' },
  { scenario: 'SELECT: select_actors_by_tag', toolName: 'control_actor', arguments: { action: 'select_actors_by_tag', tag: TAG, replaceSelection: true }, expected: 'success' },
  { scenario: 'SELECT: select_actors_in_volume', toolName: 'control_actor', arguments: { action: 'select_actors_in_volume', volumeActorName: MAIN_ACTOR, replaceSelection: true }, expected: 'success|volume not found' },
  { scenario: 'SELECT: get_selected_actors', toolName: 'control_actor', arguments: { action: 'get_selected_actors' }, expected: 'success' },
  { scenario: 'SELECT: select_all', toolName: 'control_actor', arguments: { action: 'select_all' }, expected: 'success' },
  { scenario: 'SELECT: invert_selection', toolName: 'control_actor', arguments: { action: 'invert_selection' }, expected: 'success' },
  { scenario: 'SELECT: select_children', toolName: 'control_actor', arguments: { action: 'select_children', recurseChildren: false }, expected: 'success' },
  { scenario: 'GROUP: group_actors', toolName: 'control_actor', arguments: { action: 'group_actors', actorNames: [MAIN_ACTOR, MESH_ACTOR], replaceSelection: true }, expected: 'success|group failed' },
  { scenario: 'GROUP: ungroup_actors', toolName: 'control_actor', arguments: { action: 'ungroup_actors', actorNames: [MAIN_ACTOR, MESH_ACTOR] }, expected: 'success|failed' },
  { scenario: 'GROUP: remove_selected_from_group', toolName: 'control_actor', arguments: { action: 'remove_selected_from_group' }, expected: 'success' },
  { scenario: 'GROUP: lock_selected_groups', toolName: 'control_actor', arguments: { action: 'lock_selected_groups' }, expected: 'success' },
  { scenario: 'GROUP: unlock_selected_groups', toolName: 'control_actor', arguments: { action: 'unlock_selected_groups' }, expected: 'success' },
  { scenario: 'SELECT: deselect_all', toolName: 'control_actor', arguments: { action: 'deselect_all' }, expected: 'success' },
  { scenario: 'DELETE: remove_tag', toolName: 'control_actor', arguments: actorArgs('remove_tag', { tag: TAG }), expected: 'success|not found' },
  { scenario: 'ACTION: list', toolName: 'control_actor', arguments: { action: 'list', limit: 20, filter: 'MCP_' }, expected: 'success' },

  // === MISC ===
  { scenario: 'CONFIG: set_blueprint_variables', toolName: 'control_actor', arguments: actorArgs('set_blueprint_variables', { variables: { InitialLifeSpan: 0 } }), expected: 'success' },
  { scenario: 'CREATE: create_snapshot', toolName: 'control_actor', arguments: actorArgs('create_snapshot', { snapshotName: `Snapshot_${ts}` }), expected: 'success|already exists' },
  { scenario: 'ACTION: attach', toolName: 'control_actor', arguments: { action: 'attach', childActor: CHILD_ACTOR, parentActor: PARENT_ACTOR }, expected: 'success' },
  { scenario: 'ACTION: detach', toolName: 'control_actor', arguments: { action: 'detach', actorName: CHILD_ACTOR }, expected: 'success' },
  { scenario: 'ACTION: attach_actor', toolName: 'control_actor', arguments: { action: 'attach_actor', childActor: CHILD_ACTOR, parentActor: PARENT_ACTOR }, expected: 'success' },
  { scenario: 'ACTION: detach_actor', toolName: 'control_actor', arguments: { action: 'detach_actor', actorName: CHILD_ACTOR }, expected: 'success' },
  { scenario: 'CONFIG: set_actor_collision', toolName: 'control_actor', arguments: actorArgs('set_actor_collision', { collisionEnabled: true }), expected: 'success' },
  { scenario: 'ACTION: call_actor_function', toolName: 'control_actor', arguments: actorArgs('call_actor_function', { functionName: 'SetActorTickEnabled', arguments: [true] }), expected: 'success|FUNCTION_NOT_FOUND' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete spawned actors', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [MAIN_ACTOR, DUPLICATE_ACTOR, DUPLICATE_COPY, MESH_ACTOR, PARENT_ACTOR, CHILD_ACTOR, BP_ACTOR, `MCP_SpawnSphere_${ts}`, `MCP_SpawnCylinder_${ts}`] }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('control-actor', testCases);
