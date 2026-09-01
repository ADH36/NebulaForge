import { cleanObject } from '../../utils/safe-json.js';
import { ITools, StandardActionResponse } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, normalizePathFields, requireNonEmptyString } from './common-handlers.js';

/** Extended response with common sequence fields */
interface SequenceActionResponse extends StandardActionResponse {
  result?: {
    sequencePath?: string;
    results?: Array<{ success?: boolean; error?: string }>;
    [key: string]: unknown;
  };
  bindings?: Array<{ name?: string;[key: string]: unknown }>;
  message?: string;
}

const managedSequences = new Set<string>();
const deletedSequences = new Set<string>();

function normalizeSequencePath(path: unknown): string | undefined {
  if (typeof path !== 'string') return undefined;
  const trimmed = path.trim();
  return trimmed.length > 0 ? trimmed : undefined;
}

function markSequenceCreated(path: unknown) {
  const norm = normalizeSequencePath(path);
  if (!norm) return;
  deletedSequences.delete(norm);
  managedSequences.add(norm);
}

function markSequenceDeleted(path: unknown) {
  const norm = normalizeSequencePath(path);
  if (!norm) return;
  managedSequences.delete(norm);
  deletedSequences.delete(norm);
}

function mrqState(response: Record<string, unknown>): { terminal: boolean; success: boolean } {
  const payload = response.result && typeof response.result === 'object' && !Array.isArray(response.result)
    ? response.result as Record<string, unknown>
    : response;
  const status = typeof payload.status === 'string' ? payload.status.toLowerCase() : '';
  const terminal = ['completed', 'failed', 'cancelled', 'canceled', 'error'].includes(status) || payload.completed === true;
  return { terminal, success: status === 'completed' || (terminal && payload.success === true) };
}

async function waitForMrq(tools: ITools, mrqJobId: string, timeoutMs: number, pollIntervalMs: number): Promise<Record<string, unknown>> {
  const deadline = Date.now() + timeoutMs;
  while (true) {
    const response = await executeAutomationRequest(tools, 'manage_sequence', { subAction: 'get_mrq_status', mrqJobId }) as Record<string, unknown>;
    const state = mrqState(response);
    if (state.terminal) return { ...response, terminal: true, timedOut: false, ready: state.success };
    if (Date.now() >= deadline) return { ...response, terminal: false, timedOut: true, ready: false };
    await new Promise(resolve => setTimeout(resolve, Math.min(pollIntervalMs, Math.max(1, deadline - Date.now()))));
  }
}

/** Helper to safely get string from error/message */
function getErrorString(res: SequenceActionResponse | null | undefined): string {
  if (!res) return '';
  return typeof res.error === 'string' ? res.error : '';
}

function getMessageString(res: SequenceActionResponse | null | undefined): string {
  if (!res) return '';
  return typeof res.message === 'string' ? res.message : '';
}



export async function handleSequenceTools(action: string, args: Record<string, unknown>, tools: ITools) {
  const seqAction = String(action || '').trim();
  args = normalizePathFields(args, ['path', 'destinationPath', 'subsequencePath', 'childSequencePath', 'mrqPresetPath']);
  if (seqAction === 'render_sequence_queue') {
    const queue = Array.isArray(args.queue) ? args.queue : [];
    if (queue.length === 0 || queue.length > 32) throw new Error('queue must contain between 1 and 32 render jobs');
    const waitForCompletion = args.waitForCompletion === true;
    const timeoutMs = args.timeoutMs === undefined ? 30 * 60 * 1000 : Number(args.timeoutMs);
    const pollIntervalMs = args.pollIntervalMs === undefined ? 250 : Number(args.pollIntervalMs);
    if (!Number.isFinite(timeoutMs) || timeoutMs < 1 || timeoutMs > 6 * 60 * 60 * 1000 || !Number.isFinite(pollIntervalMs) || pollIntervalMs < 25 || pollIntervalMs > 5000) {
      throw new Error('timeoutMs must be between 1 and 21600000 and pollIntervalMs between 25 and 5000');
    }
    const startedAt = Date.now();
    const results: Array<Record<string, unknown>> = [];
    for (let index = 0; index < queue.length; index++) {
      const item = queue[index];
      if (!item || typeof item !== 'object' || Array.isArray(item)) throw new Error(`queue[${index}] must be an object`);
      const submitted = await handleSequenceTools('render_sequence_mrq', item as Record<string, unknown>, tools) as Record<string, unknown>;
      results.push({ index, submitted });
      if (submitted.success === false) return { success: false, error: 'RENDER_QUEUE_SUBMISSION_FAILED', failedIndex: index, results };
      const payload = submitted.result && typeof submitted.result === 'object' && !Array.isArray(submitted.result) ? submitted.result as Record<string, unknown> : submitted;
      // Native MRQ historically returned `jobId`; accept both while keeping
      // one canonical identifier for status polling.
      const mrqJobId = typeof payload.mrqJobId === 'string'
        ? payload.mrqJobId
        : (typeof payload.jobId === 'string' ? payload.jobId : undefined);
      if (waitForCompletion && !mrqJobId) {
        return { success: false, error: 'RENDER_QUEUE_MISSING_JOB_ID', failedIndex: index, results };
      }
      if (waitForCompletion && mrqJobId) {
        const remaining = timeoutMs - (Date.now() - startedAt);
        if (remaining <= 0) return { success: false, error: 'RENDER_QUEUE_TIMEOUT', timedOut: true, failedIndex: index, results };
        const terminal = await waitForMrq(tools, mrqJobId, remaining, pollIntervalMs);
        results[results.length - 1] = { index, submitted, terminal };
        if (terminal.ready !== true) return { success: false, error: terminal.timedOut === true ? 'RENDER_QUEUE_TIMEOUT' : 'RENDER_QUEUE_JOB_FAILED', failedIndex: index, results };
      }
    }
    return { success: true, queuedJobs: results.length, completedJobs: waitForCompletion ? results.length : 0, results };
  }
  switch (seqAction) {
    case 'create':
    case 'create_master_sequence': {
      const name = requireNonEmptyString(args.name, 'name', 'Missing required parameter: name');
      const basePath = typeof args.path === 'string' ? args.path.trim().replace(/\/$/, '') : '/Game/Sequences';

      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        name,
        path: basePath,
        subAction: 'create'
      }) as SequenceActionResponse;

      let sequencePath: string | undefined;
      if (res && res.result && typeof res.result.sequencePath === 'string') {
        sequencePath = res.result.sequencePath;
      } else if (typeof args.path === 'string' && args.path.trim().length > 0) {
        const p = args.path.trim().replace(/\/$/, '');
        sequencePath = `${p}/${name}`;
      }
      if (sequencePath && res && res.success !== false) {
        markSequenceCreated(sequencePath);
      }

      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (res && res.success === false && (errorCode === 'FACTORY_NOT_AVAILABLE' || msgLower.includes('ulevelsequencefactorynew not available'))) {
        const path = sequencePath || (typeof args.path === 'string' ? args.path : undefined);
        return cleanObject({
          success: false,
          error: 'FACTORY_NOT_AVAILABLE',
          message: res.message || 'Sequence creation failed: factory not available',
          action: 'create',
          name,
          path,
          sequencePath,
          handled: true
        });
      }

      return cleanObject(res);
    }
    case 'open': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'open'
      }) as SequenceActionResponse;
      return cleanObject(res);
    }
    case 'add_camera':
    case 'create_cine_camera_actor': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        spawnable: args.spawnable !== false,
        subAction: 'add_camera'
      }) as SequenceActionResponse;
      return cleanObject(res);
    }
    case 'add_subsequence': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const subsequencePath = requireNonEmptyString(args.subsequencePath ?? args.childSequencePath, 'subsequencePath', 'Missing required parameter: subsequencePath');
      const durationFrames = Number(args.durationFrames);
      if (!Number.isInteger(durationFrames) || durationFrames < 1 || durationFrames > 10000000) {
        throw new Error('durationFrames must be an integer between 1 and 10000000');
      }
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subsequencePath,
        durationFrames,
        subAction: 'add_subsequence'
      }) as SequenceActionResponse;
      return cleanObject(res);
    }
    case 'inspect_shot_settings': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const shotIndex = Number(args.shotIndex ?? 0);
      if (!Number.isInteger(shotIndex) || shotIndex < 0 || shotIndex > 100000) {
        throw new Error('shotIndex must be a non-negative integer');
      }
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
        ...args, path, shotIndex, subAction: 'inspect_shot_settings'
      }) as SequenceActionResponse);
    }
    case 'add_audio_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const soundPath = requireNonEmptyString(args.soundPath, 'soundPath', 'Missing required parameter: soundPath');
      const frame = Number(args.frame ?? 0);
      const durationFrames = Number(args.durationFrames ?? 1);
      const rowIndex = Number(args.rowIndex ?? 0);
      if (!Number.isInteger(frame) || frame < -1000000000 || frame > 1000000000) throw new Error('frame must be an integer between -1000000000 and 1000000000');
      if (!Number.isInteger(durationFrames) || durationFrames < 1 || durationFrames > 10000000) throw new Error('durationFrames must be an integer between 1 and 10000000');
      if (!Number.isInteger(rowIndex) || rowIndex < 0 || rowIndex > 100000) throw new Error('rowIndex must be a non-negative integer');
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
        ...args, path, soundPath, frame, durationFrames, rowIndex, subAction: 'add_audio_track'
      }) as SequenceActionResponse);
    }
    case 'add_material_parameter_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const parameterName = requireNonEmptyString(args.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      const frame = Number(args.frame ?? 0);
      const value = Number(args.value);
      const rowIndex = Number(args.rowIndex ?? 0);
      if (!Number.isInteger(frame) || frame < -1000000000 || frame > 1000000000) throw new Error('frame must be an integer between -1000000000 and 1000000000');
      if (!Number.isFinite(value)) throw new Error('value must be a finite number');
      if (!Number.isInteger(rowIndex) || rowIndex < 0 || rowIndex > 100000) throw new Error('rowIndex must be a non-negative integer');
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
        ...args, path, actorName, parameterName, frame, value, rowIndex, subAction: 'add_material_parameter_track'
      }) as SequenceActionResponse);
    }
    case 'add_material_color_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const parameterName = requireNonEmptyString(args.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      const frame = Number(args.frame ?? 0);
      const rowIndex = Number(args.rowIndex ?? 0);
      const channels = ['colorR', 'colorG', 'colorB', 'colorA'].map((key) => Number(args[key] ?? (key === 'colorA' ? 1 : 0)));
      if (!Number.isInteger(frame) || frame < -1000000000 || frame > 1000000000) throw new Error('frame must be an integer between -1000000000 and 1000000000');
      if (!channels.every((channel) => Number.isFinite(channel))) throw new Error('color channels must be finite numbers');
      if (!Number.isInteger(rowIndex) || rowIndex < 0 || rowIndex > 100000) throw new Error('rowIndex must be a non-negative integer');
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
        ...args, path, actorName, parameterName, frame, rowIndex,
        colorR: channels[0], colorG: channels[1], colorB: channels[2], colorA: channels[3],
        subAction: 'add_material_color_track'
      }) as SequenceActionResponse);
    }
    case 'add_custom_primitive_data_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const customPrimitiveDataIndex = Number(args.customPrimitiveDataIndex);
      const frame = Number(args.frame ?? 0);
      const value = Number(args.value);
      const rowIndex = Number(args.rowIndex ?? 0);
      if (!Number.isInteger(customPrimitiveDataIndex) || customPrimitiveDataIndex < 0 || customPrimitiveDataIndex > 255) throw new Error('customPrimitiveDataIndex must be an integer between 0 and 255');
      if (!Number.isInteger(frame) || frame < -1000000000 || frame > 1000000000) throw new Error('frame must be an integer between -1000000000 and 1000000000');
      if (!Number.isFinite(value)) throw new Error('value must be a finite number');
      if (!Number.isInteger(rowIndex) || rowIndex < 0 || rowIndex > 100000) throw new Error('rowIndex must be a non-negative integer');
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
        ...args, path, actorName, customPrimitiveDataIndex, frame, value, rowIndex,
        subAction: 'add_custom_primitive_data_track'
      }) as SequenceActionResponse);
    }
    case 'add_niagara_system_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const startFrame = Number(args.startFrame ?? 0);
      const endFrame = Number(args.endFrame ?? startFrame + 1);
      if (!Number.isInteger(startFrame) || !Number.isInteger(endFrame) || endFrame <= startFrame) throw new Error('endFrame must be an integer greater than startFrame');
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
        ...args, path, actorName, startFrame, endFrame, subAction: 'add_niagara_system_track'
      }) as SequenceActionResponse);
    }
    case 'create_niagara_float_parameter_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const parameterName = requireNonEmptyString(args.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
        ...args, path, actorName, parameterName, subAction: 'create_niagara_float_parameter_track'
      }) as SequenceActionResponse);
    }
    case 'add_niagara_float_parameter_key': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const parameterName = requireNonEmptyString(args.parameterName, 'parameterName', 'Missing required parameter: parameterName');
      const frame = Number(args.frame ?? 0);
      const value = Number(args.value);
      const rowIndex = Number(args.rowIndex ?? 0);
      if (!Number.isInteger(frame) || frame < -1000000000 || frame > 1000000000) throw new Error('frame must be an integer between -1000000000 and 1000000000');
      if (!Number.isFinite(value)) throw new Error('value must be a finite number');
      if (!Number.isInteger(rowIndex) || rowIndex < 0 || rowIndex > 100000) throw new Error('rowIndex must be a non-negative integer');
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
        ...args, path, actorName, parameterName, frame, value, rowIndex, subAction: 'add_niagara_float_parameter_key'
      }) as SequenceActionResponse);
    }
    case 'inspect_niagara_parameter_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
        ...args, path, actorName, subAction: 'inspect_niagara_parameter_track'
      }) as SequenceActionResponse);
    }
    case 'add_shot_track':
    case 'configure_shot_settings':
    case 'add_camera_cut_track':
    case 'add_camera_shake_track':
    case 'add_fade_track':
    case 'add_level_visibility_track':
    case 'add_skeletal_animation_track':
    case 'add_transform_track':
    case 'add_event_track':
    case 'add_property_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      if (seqAction === 'configure_shot_settings') {
        const shotIndex = Number(args.shotIndex ?? 0);
        if (!Number.isInteger(shotIndex) || shotIndex < 0 || shotIndex > 100000) {
          throw new Error('shotIndex must be a non-negative integer');
        }
        const settings = ['shotDisplayName', 'thumbnailReferenceOffset', 'startFrame', 'endFrame']
          .filter((field) => args[field] !== undefined);
        if (settings.length === 0) throw new Error('At least one shot setting must be provided');
        return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
          ...args, path, shotIndex, subAction: 'configure_shot_settings'
        }) as SequenceActionResponse);
      }
      const trackTypes: Record<string, string> = {
        add_shot_track: 'CinematicShot',
        add_camera_cut_track: 'CameraCut',
        add_camera_shake_track: 'CameraShake',
        add_fade_track: 'Fade',
        add_level_visibility_track: 'LevelVisibility',
        add_skeletal_animation_track: 'SkeletalAnimation',
        add_transform_track: '3DTransform',
        add_event_track: 'Event',
        add_property_track: 'Property'
      };
      const trackType = trackTypes[seqAction];
      if (seqAction === 'add_camera_shake_track') {
        const shakeClass = requireNonEmptyString(args.shakeClass, 'shakeClass', 'Missing required parameter: shakeClass');
        const frame = Number(args.frame ?? 0);
        if (!Number.isInteger(frame) || frame < -1000000000 || frame > 1000000000) {
          throw new Error('frame must be an integer between -1000000000 and 1000000000');
        }
        const shakeRes = await executeAutomationRequest(tools, 'manage_sequence', {
          ...args,
          path,
          trackType,
          shakeClass,
          frame,
          subAction: 'add_camera_shake_track'
        }) as SequenceActionResponse;
        return cleanObject(shakeRes);
      }
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        trackType,
        subAction: 'add_track'
      }) as SequenceActionResponse;
      return cleanObject(res);
    }
    case 'add_actor': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const path = typeof args.path === 'string' ? args.path.trim() : '';
      const payload = {
        ...args,
        actorName,
        path: path || args.path,
        subAction: 'add_actor'
      };

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload) as SequenceActionResponse;

      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();

      if (res && res.success === false && path) {
        const isInvalidSequence = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence_add_actor requires a sequence path') || msgLower.includes('sequence not found');
        if (isInvalidSequence) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            message: res.message || 'Sequence not found',
            action: 'add_actor',
            path,
            actorName
          });
        }
      }

      const results = res && res.result && Array.isArray(res.result.results)
        ? res.result.results
        : undefined;
      if (results && results.length) {
        const failed = results.find((item) => item && item.success === false && typeof item.error === 'string');
        if (failed) {
          const errText = String(failed.error).toLowerCase();
          if (errText.includes('actor not found')) {
            return cleanObject({
              success: false,
              error: 'NOT_FOUND',
              message: failed.error,
              action: 'add_actor',
              path: path || undefined,
              actorName
            });
          }
        }
      }

      return cleanObject(res);
    }
    case 'add_actors': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames as string[] : [];
      if (actorNames.length === 0) {
        throw new Error('Missing required parameter: actorNames (must be non-empty array)');
      }

      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        actorNames,
        path,
        subAction: 'add_actors'
      }) as SequenceActionResponse;

      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (actorNames.length === 0 && res && res.success === false && errorCode === 'INVALID_ARGUMENT') {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: res.message || 'Invalid argument: actorNames required',
          action: 'add_actors',
          actorNames
        });
      }
      if (res && res.success === false && msgLower.includes('actor not found')) {
        return cleanObject({
          success: false,
          error: 'NOT_FOUND',
          message: res.message || 'Actor not found',
          action: 'add_actors',
          actorNames
        });
      }
      return cleanObject(res);
    }
    case 'remove_actors': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames as string[] : [];
      if (actorNames.length === 0) {
        throw new Error('Missing required parameter: actorNames (must be non-empty array)');
      }
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        actorNames,
        path,
        subAction: 'remove_actors'
      });
      return cleanObject(res);
    }
    case 'get_bindings': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'get_bindings'
      });
      return cleanObject(res);
    }
    case 'add_keyframe': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const property = typeof args.property === 'string' ? args.property : 'Transform';
      const frame = typeof args.frame === 'number' ? args.frame : Number(args.frame);
      if (!Number.isFinite(frame)) {
        throw new Error('Missing or invalid required parameter: frame (must be a number)');
      }

      const payload: Record<string, unknown> = {
        ...args,
        path,
        actorName,
        property,
        frame,
        subAction: 'add_keyframe'
      };

      // Fix: Map common property names to internal names
      if (property === 'Location') {
        payload.property = 'Transform';
        payload.value = { location: args.value };
      } else if (property === 'Rotation') {
        payload.property = 'Transform';
        payload.value = { rotation: args.value };
      } else if (property === 'Scale') {
        payload.property = 'Transform';
        payload.value = { scale: args.value };
      }

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload) as SequenceActionResponse;
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();

      // Keep explicit INVALID_ARGUMENT for missing frame as a real error
      if (errorCode === 'INVALID_ARGUMENT' || msgLower.includes('frame number is required')) {
        return cleanObject(res);
      }

      if (res && res.success === false) {
        const isBindingIssue = errorCode === 'BINDING_NOT_FOUND' || msgLower.includes('binding not found');
        const isUnsupported = errorCode === 'UNSUPPORTED_PROPERTY' || msgLower.includes('unsupported property') || msgLower.includes('invalid_sequence_type');
        const isInvalidSeq = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence not found') || msgLower.includes('requires a sequence path');

        if (path && isInvalidSeq) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            message: res.message || 'Sequence not found',
            action: 'add_keyframe',
            path,
            actorName,
            property,
            frame
          });
        }

        // Preserve plugin-provided failure for binding / unsupported-property cases
        if (path && (isBindingIssue || isUnsupported)) {
          return cleanObject(res);
        }
      }

      return cleanObject(res);
    }
    case 'add_spawnable_from_class': {
      const className = requireNonEmptyString(args.className, 'className', 'Missing required parameter: className');
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        className,
        path,
        subAction: 'add_spawnable_from_class'
      });
      return cleanObject(res);
    }
    case 'play': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        startTime: args.startTime as number | undefined,
        loopMode: args.loopMode as 'once' | 'loop' | 'pingpong' | undefined,
        subAction: 'play'
      });
      return cleanObject(res);
    }
    case 'pause': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'pause'
      });
      return cleanObject(res);
    }
    case 'stop': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'stop'
      });
      return cleanObject(res);
    }
    case 'set_properties': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        frameRate: args.frameRate as number | undefined,
        lengthInFrames: args.lengthInFrames as number | undefined,
        playbackStart: args.playbackStart as number | undefined,
        playbackEnd: args.playbackEnd as number | undefined,
        subAction: 'set_properties'
      });
      return cleanObject(res);
    }
    case 'get_properties': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'get_properties'
      });
      return cleanObject(res);
    }
    case 'set_playback_speed': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const speed = Number(args.speed);
      if (!Number.isFinite(speed) || speed <= 0) {
        throw new Error('Invalid speed: must be a positive number');
      }

      // Try setting speed
      let res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        speed,
        subAction: 'set_playback_speed'
      }) as SequenceActionResponse;

      // Fix: Auto-open if editor not open
      const errorCode = getErrorString(res).toUpperCase();
      if ((!res || res.success === false) && errorCode === 'EDITOR_NOT_OPEN') {
        // Attempt to open the sequence
        await executeAutomationRequest(tools, 'manage_sequence', {
          path,
          subAction: 'open'
        });

        // Wait a short moment for editor to initialize on game thread
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Retry
        res = await executeAutomationRequest(tools, 'manage_sequence', {
          ...args,
          path,
          speed,
          subAction: 'set_playback_speed'
        }) as SequenceActionResponse;
      }

      return cleanObject(res);
    }
    case 'list': {
      const path = typeof args.path === 'string' ? args.path.trim() : undefined;
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'list'
      });
      return cleanObject(res);
    }
    case 'duplicate': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const destDir = requireNonEmptyString(args.destinationPath, 'destinationPath', 'Missing required parameter: destinationPath');
      const defaultNewName = path.split('/').pop() || '';
      const newName = requireNonEmptyString(args.newName || defaultNewName, 'newName', 'Missing required parameter: newName');
      const baseDir = destDir.replace(/\/$/, '');
      const destPath = `${baseDir}/${newName}`;
      // Pass newName separately for C++ handler to use
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        subAction: 'duplicate',
        path,
        destinationPath: destPath,
        newName
      });
      return cleanObject(res);
    }
    case 'rename': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const newName = requireNonEmptyString(args.newName, 'newName', 'Missing required parameter: newName');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        newName,
        subAction: 'rename'
      }) as SequenceActionResponse;
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (res && res.success === false && (errorCode === 'OPERATION_FAILED' || msgLower.includes('failed to rename sequence'))) {
        // Return actual failure, not best-effort success - rename is a destructive operation
        return cleanObject({
          success: false,
          error: 'OPERATION_FAILED',
          message: res.message || 'Failed to rename sequence',
          action: 'rename',
          path,
          newName
        });
      }
      return cleanObject(res);
    }
    case 'delete': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'delete'
      }) as SequenceActionResponse;

      if (res && res.success !== false) {
        markSequenceDeleted(path);
      }
      return cleanObject(res);
    }
    case 'get_metadata': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'get_metadata'
      });
      return cleanObject(res);
    }
    case 'set_metadata': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata as Record<string, unknown> : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: path, metadata });
      return cleanObject(res);
    }
    case 'add_track': {
      // Forward add_track to the C++ plugin - it requires MovieScene API
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const trackType = requireNonEmptyString(args.trackType, 'trackType', 'Missing required parameter: trackType');
      const trackName = typeof args.trackName === 'string' ? args.trackName : '';
      const actorName = typeof args.actorName === 'string' ? args.actorName : undefined;

      // Fix: Check if actor is bound before adding track
      if (actorName) {
        const bindingsRes = await executeAutomationRequest(tools, 'manage_sequence', {
          path,
          subAction: 'get_bindings'
        }) as SequenceActionResponse;
        if (bindingsRes && bindingsRes.success) {
          const bindings = bindingsRes.bindings || [];
          const isBound = bindings.some((b) => b.name === actorName);
          if (!isBound) {
            return cleanObject({
              success: false,
              error: 'BINDING_NOT_FOUND',
              message: `Actor '${actorName}' is not bound to this sequence. Please call 'add_actor' first.`,
              action: 'add_track',
              path,
              actorName
            });
          }
        }
      }

      const payload = {
        ...args,
        path,
        trackType,
        trackName,
        actorName,
        subAction: 'add_track'
      };

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload);
      return cleanObject(res);
    }
    case 'add_section': {
      // Forward add_section to C++
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = { ...args, path, subAction: 'add_section' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'remove_track': {
      // Forward remove_track to C++
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const trackName = requireNonEmptyString(args.trackName, 'trackName', 'Missing required parameter: trackName');
      const payload = { ...args, path, trackName, subAction: 'remove_track' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'set_track_muted': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const trackName = requireNonEmptyString(args.trackName, 'trackName', 'Missing required parameter: trackName');
      const payload = { ...args, path, trackName, subAction: 'set_track_muted' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'set_track_solo': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const trackName = requireNonEmptyString(args.trackName, 'trackName', 'Missing required parameter: trackName');
      const payload = { ...args, path, trackName, subAction: 'set_track_solo' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'set_track_locked': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const trackName = requireNonEmptyString(args.trackName, 'trackName', 'Missing required parameter: trackName');
      const payload = { ...args, path, trackName, subAction: 'set_track_locked' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'list_tracks': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'list_tracks'
      });
      return cleanObject(res);
    }
	case 'set_work_range': {
		const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
		const start = Number(args.start);
		const end = Number(args.end);
		// Validate start/end are numbers
		if (!Number.isFinite(start)) throw new Error('Invalid start: must be a number');
		if (!Number.isFinite(end)) throw new Error('Invalid end: must be a number');

		const res = await executeAutomationRequest(tools, 'manage_sequence', {
			...args,
			path,
			start,
			end,
			subAction: 'set_work_range'
		});
		return cleanObject(res);
	}
	case 'set_tick_resolution': {
		const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
		const resolution = args.resolution;
		if (resolution === undefined || resolution === null) {
			throw new Error('Missing required parameter: resolution');
		}

		const res = await executeAutomationRequest(tools, 'manage_sequence', {
			...args,
			path,
			resolution,
			subAction: 'set_tick_resolution'
		});
		return cleanObject(res);
	}
	case 'set_view_range': {
		const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
		const start = args.start !== undefined ? Number(args.start) : undefined;
		const end = args.end !== undefined ? Number(args.end) : undefined;

		const res = await executeAutomationRequest(tools, 'manage_sequence', {
			...args,
			path,
			start,
			end,
			subAction: 'set_view_range'
		});
		return cleanObject(res);
	}
	case 'render_sequence_mrq': {
		const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
		const outputPath = requireNonEmptyString(args.outputPath, 'outputPath', 'Missing required parameter: outputPath');
		if (outputPath.startsWith('/') || /^[A-Za-z]:[\\/]/.test(outputPath) || outputPath.includes('..')) {
			throw new Error('outputPath must be a project-relative directory');
		}
		for (const field of ['spatialSampleCount', 'temporalSampleCount']) {
			if (args[field] !== undefined && (!Number.isInteger(Number(args[field])) || Number(args[field]) < 1 || Number(args[field]) > 256)) {
				throw new Error(`${field} must be an integer between 1 and 256`);
			}
		}
		if (args.renderPass !== undefined) {
			const renderPass = String(args.renderPass).trim().toLowerCase();
			if (!['beauty', 'object_id'].includes(renderPass)) throw new Error('renderPass must be beauty or object_id');
		}
		return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
			...args, path, outputPath, subAction: 'render_sequence_mrq'
		}));
	}
	case 'configure_burn_ins': {
		const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
		const outputPath = requireNonEmptyString(args.outputPath, 'outputPath', 'Missing required parameter: outputPath');
		if (args.burnInClass !== undefined && (typeof args.burnInClass !== 'string' || args.burnInClass.trim().length === 0)) {
			throw new Error('burnInClass must be a non-empty class path');
		}
		return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
			...args, path, outputPath, subAction: 'render_sequence_mrq'
		}));
	}
	case 'get_mrq_status':
		return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
			...args, subAction: 'get_mrq_status'
		}));
	case 'cancel_mrq':
		return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', {
			...args, subAction: 'cancel_mrq'
		}));
    default:
      // Ensure subAction is set for compatibility with C++ handler expectations
      const payload = { ...args };
      if (payload.action && !payload.subAction) {
        payload.subAction = payload.action;
      }
      return await executeAutomationRequest(tools, 'manage_sequence', payload);
  }
}
