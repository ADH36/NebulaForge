import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true }))
}));

vi.mock('./common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('./common-handlers.js')>('./common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock
  };
});

import { handleSequenceTools } from './sequence-handlers.js';

describe('handleSequenceTools path normalization', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('normalizes sequence creation path aliases before dispatch', async () => {
    await handleSequenceTools('create', {
      action: 'create',
      name: 'SEQ_Test',
      path: 'Game/MCPTest/Sequences'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_sequence',
      expect.objectContaining({
        subAction: 'create',
        path: '/Game/MCPTest/Sequences'
      })
    );
  });

  it('normalizes duplicate source and destination path aliases before dispatch', async () => {
    await handleSequenceTools('duplicate', {
      action: 'duplicate',
      path: 'Game/MCPTest/Sequences/SEQ_Test',
      destinationPath: 'Content/MCPTest/Duplicates',
      newName: 'SEQ_Copy'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_sequence',
      expect.objectContaining({
        subAction: 'duplicate',
        path: '/Game/MCPTest/Sequences/SEQ_Test',
        destinationPath: '/Game/MCPTest/Duplicates/SEQ_Copy'
      })
    );
  });

  it('maps cinematic aliases to the existing safe sequence primitives', async () => {
    await handleSequenceTools('create_master_sequence', {
      name: 'SEQ_Master',
      path: 'Game/MCPTest/Sequences'
    }, {} as never);
    await handleSequenceTools('create_cine_camera_actor', {
      path: 'Game/MCPTest/Sequences/SEQ_Master',
      spawnable: true
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenNthCalledWith(
      1,
      {},
      'manage_sequence',
      expect.objectContaining({ subAction: 'create', path: '/Game/MCPTest/Sequences' })
    );
    expect(executeAutomationRequestMock).toHaveBeenNthCalledWith(
      2,
      {},
      'manage_sequence',
      expect.objectContaining({ subAction: 'add_camera', path: '/Game/MCPTest/Sequences/SEQ_Master', spawnable: true })
    );
  });

  it('validates and dispatches subsequence composition parameters', async () => {
    await handleSequenceTools('add_subsequence', {
      path: 'Game/MCPTest/Sequences/SEQ_Master',
      subsequencePath: 'Game/MCPTest/Sequences/SEQ_Shot',
      startFrame: 24,
      durationFrames: 120,
      rowIndex: 1
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_sequence',
      expect.objectContaining({
        subAction: 'add_subsequence',
        path: '/Game/MCPTest/Sequences/SEQ_Master',
        subsequencePath: '/Game/MCPTest/Sequences/SEQ_Shot',
        durationFrames: 120,
        rowIndex: 1
      })
    );
    await expect(handleSequenceTools('add_subsequence', {
      path: '/Game/MCPTest/Sequences/SEQ_Master',
      subsequencePath: '/Game/MCPTest/Sequences/SEQ_Shot',
      durationFrames: 0
    }, {} as never)).rejects.toThrow(/durationFrames/);
  });

  it('maps canonical cinematic track names to the generic native track authoring path', async () => {
    await handleSequenceTools('add_fade_track', {
      path: 'Game/MCPTest/Sequences/SEQ_Master'
    }, {} as never);
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_sequence',
      expect.objectContaining({ subAction: 'add_track', trackType: 'Fade', path: '/Game/MCPTest/Sequences/SEQ_Master' })
    );
  });

  it('submits a bounded ordered render queue', async () => {
    executeAutomationRequestMock.mockResolvedValue({ success: true, result: { jobId: 'mrq-test' } });
    const result = await handleSequenceTools('render_sequence_queue', {
      queue: [
        { path: '/Game/Cinematics/Intro', outputPath: 'Saved/Renders/Intro' },
        { path: '/Game/Cinematics/Outro', outputPath: 'Saved/Renders/Outro' }
      ],
      waitForCompletion: false
    }, {} as never);
    expect(result).toMatchObject({ success: true, queuedJobs: 2, completedJobs: 0 });
    expect(executeAutomationRequestMock).toHaveBeenCalledTimes(2);
  });

  it('fails a waiting render queue when native submission omits its job id', async () => {
    executeAutomationRequestMock.mockResolvedValue({ success: true, result: { status: 'queued' } });
    const result = await handleSequenceTools('render_sequence_queue', {
      queue: [{ path: '/Game/Cinematics/Intro', outputPath: 'Saved/Renders/Intro' }],
      waitForCompletion: true,
      timeoutMs: 1000
    }, {} as never);
    expect(result).toMatchObject({ success: false, error: 'RENDER_QUEUE_MISSING_JOB_ID' });
  });

  it('forwards deterministic MRQ resolution and frame range settings', async () => {
    executeAutomationRequestMock.mockResolvedValue({ success: true, result: { mrqJobId: 'mrq-configured' } });
    await handleSequenceTools('render_sequence_mrq', {
      path: '/Game/Cinematics/Intro',
      outputPath: 'Saved/Renders/Intro',
      outputFormat: 'exr',
      resolution: '1920x1080',
      startFrame: 10,
      endFrame: 120
    }, {} as never);
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_sequence',
      expect.objectContaining({
        subAction: 'render_sequence_mrq',
        resolution: '1920x1080',
        outputFormat: 'exr',
        startFrame: 10,
        endFrame: 120
      })
    );
  });

  it('forwards an MRQ preset asset without rewriting it', async () => {
    executeAutomationRequestMock.mockResolvedValue({ success: true, result: { mrqJobId: 'mrq-preset' } });
    await handleSequenceTools('render_sequence_mrq', {
      path: '/Game/Cinematics/Intro',
      outputPath: 'Saved/Renders/Intro',
      mrqPresetPath: '/Game/Cinematics/Presets/Default'
    }, {} as never);
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_sequence',
      expect.objectContaining({
        subAction: 'render_sequence_mrq',
        mrqPresetPath: '/Game/Cinematics/Presets/Default'
      })
    );
  });

  it('treats a native terminal MRQ completion as successful', async () => {
    executeAutomationRequestMock
      .mockResolvedValueOnce({ success: true, result: { jobId: 'mrq-complete' } })
      .mockResolvedValueOnce({ success: true, result: { mrqJobId: 'mrq-complete', status: 'completed', completed: true, success: true } });
    const result = await handleSequenceTools('render_sequence_queue', {
      queue: [{ path: '/Game/Cinematics/Intro', outputPath: 'Saved/Renders/Intro' }],
      waitForCompletion: true,
      timeoutMs: 1000,
      pollIntervalMs: 25
    }, {} as never);
    expect(result).toMatchObject({ success: true, queuedJobs: 1, completedJobs: 1 });
  });
});
