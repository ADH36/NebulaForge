import { spawn } from 'node:child_process';
import { describe, expect, it } from 'vitest';
import { jobManager } from './job-manager.js';

async function waitForTerminal(jobId: string): Promise<ReturnType<typeof jobManager.get>> {
  for (let attempt = 0; attempt < 50; attempt += 1) {
    const job = jobManager.get(jobId);
    if (job && ['completed', 'failed', 'cancelled'].includes(job.status)) return job;
    await new Promise(resolve => setTimeout(resolve, 10));
  }
  return jobManager.get(jobId);
}

describe('JobManager', () => {
  it('tracks process output and final exit state', async () => {
    const child = spawn(process.execPath, ['-e', "process.stdout.write('ok'); process.stderr.write('warn')"], {
      stdio: ['ignore', 'pipe', 'pipe']
    });
    const started = jobManager.startProcess({ label: 'test-success', process: child });
    const finished = await waitForTerminal(started.jobId);

    expect(finished).toMatchObject({
      jobId: started.jobId,
      label: 'test-success',
      status: 'completed',
      exitCode: 0,
      output: 'ok',
      errorOutput: 'warn'
    });
  });

  it('invokes completion callbacks with terminal output', async () => {
    const child = spawn(process.execPath, ['-e', "process.stdout.write('report-ready')"], {
      stdio: ['ignore', 'pipe', 'pipe']
    });
    let callbackJob: ReturnType<typeof jobManager.get>;
    const started = jobManager.startProcess({
      label: 'test-callback',
      process: child,
      onComplete: (job) => { callbackJob = job; }
    });
    await waitForTerminal(started.jobId);

    expect(callbackJob).toMatchObject({ status: 'completed', output: 'report-ready' });
  });

  it('exposes pending and failed completion-callback state', async () => {
    const child = spawn(process.execPath, ['-e', 'process.stdout.write("callback-state")'], {
      stdio: ['ignore', 'pipe', 'pipe']
    });
    let releaseCallback!: () => void;
    const callbackDone = new Promise<void>(resolve => { releaseCallback = resolve; });
    const started = jobManager.startProcess({
      label: 'test-callback-state',
      process: child,
      onComplete: async () => callbackDone
    });
    const finished = await waitForTerminal(started.jobId);
    expect(finished).toMatchObject({ status: 'completed', completionPending: true });
    releaseCallback();
    for (let attempt = 0; attempt < 50; attempt += 1) {
      if (jobManager.get(started.jobId)?.completionPending === false) break;
      await new Promise(resolve => setTimeout(resolve, 10));
    }
    expect(jobManager.get(started.jobId)).toMatchObject({ completionPending: false });
  });

  it('cancels a running process and retains its terminal status', async () => {
    const child = spawn(process.execPath, ['-e', 'setTimeout(() => {}, 10000)'], {
      stdio: ['ignore', 'pipe', 'pipe']
    });
    const started = jobManager.startProcess({ label: 'test-cancel', process: child });
    const cancelled = jobManager.cancel(started.jobId);

    expect(cancelled).toMatchObject({ jobId: started.jobId, status: 'cancelled' });
    const finished = await waitForTerminal(started.jobId);
    expect(finished?.status).toBe('cancelled');
  });

  it('fails and terminates a process that exceeds its deadline', async () => {
    const child = spawn(process.execPath, ['-e', 'setTimeout(() => {}, 10000)'], {
      stdio: ['ignore', 'pipe', 'pipe']
    });
    const started = jobManager.startProcess({ label: 'test-timeout', process: child, timeoutMs: 25 });
    const finished = await waitForTerminal(started.jobId);

    expect(finished).toMatchObject({
      jobId: started.jobId,
      status: 'failed',
      error: 'Job exceeded timeout of 25ms'
    });
  });

  it('waits for a managed job to reach a terminal state', async () => {
    const child = spawn(process.execPath, ['-e', 'setTimeout(() => process.stdout.write("done"), 20)'], {
      stdio: ['ignore', 'pipe', 'pipe']
    });
    const started = jobManager.startProcess({ label: 'test-wait', process: child });
    const waited = await jobManager.waitForTerminal(started.jobId, 1000);
    expect(waited.timedOut).toBe(false);
    expect(waited.job).toMatchObject({ status: 'completed', output: 'done' });
  });

  it('reports a bounded wait timeout without losing the job', async () => {
    const child = spawn(process.execPath, ['-e', 'setTimeout(() => {}, 10000)'], {
      stdio: ['ignore', 'pipe', 'pipe']
    });
    const started = jobManager.startProcess({ label: 'test-wait-timeout', process: child });
    const waited = await jobManager.waitForTerminal(started.jobId, 1);
    expect(waited.timedOut).toBe(true);
    expect(waited.job?.status).toBe('running');
    jobManager.cancel(started.jobId);
  });

  it('tracks non-process tasks and supports cooperative cancellation', async () => {
    let observedAbort = false;
    const started = jobManager.startTask({
      label: 'test-task',
      task: async (signal) => {
        await new Promise(resolve => setTimeout(resolve, 100));
        observedAbort = signal.aborted;
        if (signal.aborted) throw new Error('task observed cancellation');
      }
    });
    const cancelled = jobManager.cancel(started.jobId);
    expect(cancelled).toMatchObject({ jobId: started.jobId, status: 'cancelled' });
    await new Promise(resolve => setTimeout(resolve, 125));
    expect(observedAbort).toBe(true);
    expect(jobManager.get(started.jobId)?.status).toBe('cancelled');
  });
});
