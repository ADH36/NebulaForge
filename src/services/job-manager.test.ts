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
});
