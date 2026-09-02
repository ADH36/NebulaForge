import { randomUUID } from 'node:crypto';
import type { ChildProcessWithoutNullStreams } from 'node:child_process';

export type JobStatus = 'queued' | 'running' | 'completed' | 'failed' | 'cancelled';

export interface JobSnapshot {
  jobId: string;
  label: string;
  status: JobStatus;
  startedAt: string;
  finishedAt?: string;
  exitCode?: number | null;
  signal?: string | null;
  output: string;
  errorOutput: string;
  outputTruncated: boolean;
  error?: string;
  timedOut?: boolean;
  completionPending?: boolean;
  completionError?: string;
}

interface ManagedJob extends JobSnapshot {
  process?: ChildProcessWithoutNullStreams;
  deadlineTimer?: NodeJS.Timeout;
  abortController?: AbortController;
}

const MAX_OUTPUT_SIZE = 256 * 1024;
const MAX_JOBS = 100;

/**
 * Process-scoped lifecycle registry for long-running host operations.
 * It deliberately owns only child processes started by the MCP server.
 */
class JobManager {
  private readonly jobs = new Map<string, ManagedJob>();

  public startProcess(options: {
    label: string;
    process: ChildProcessWithoutNullStreams;
    timeoutMs?: number;
    onComplete?: (job: JobSnapshot) => void | Promise<void>;
  }): JobSnapshot {
    this.pruneFinishedJobs();
    const job: ManagedJob = {
      jobId: randomUUID(),
      label: options.label,
      status: 'running',
      startedAt: new Date().toISOString(),
      output: '',
      errorOutput: '',
      outputTruncated: false,
      completionPending: Boolean(options.onComplete),
      process: options.process,
    };
    this.jobs.set(job.jobId, job);

    if (options.timeoutMs !== undefined && Number.isFinite(options.timeoutMs) && options.timeoutMs > 0) {
      job.deadlineTimer = setTimeout(() => {
        if (job.status !== 'running' && job.status !== 'queued') return;
        job.status = 'failed';
        job.timedOut = true;
        job.error = `Job exceeded timeout of ${options.timeoutMs}ms`;
        job.finishedAt = new Date().toISOString();
        job.process?.kill();
      }, options.timeoutMs);
    }

    options.process.stdout.on('data', (chunk: Buffer | string) => {
      this.appendOutput(job, 'output', chunk.toString());
    });
    options.process.stderr.on('data', (chunk: Buffer | string) => {
      this.appendOutput(job, 'errorOutput', chunk.toString());
    });
    options.process.once('error', (error: Error) => {
      if (job.deadlineTimer) clearTimeout(job.deadlineTimer);
      if (job.status !== 'running' && job.status !== 'queued') return;
      job.status = 'failed';
      job.error = error.message;
      job.finishedAt = new Date().toISOString();
    });
    options.process.once('close', (code: number | null, signal: NodeJS.Signals | null) => {
      if (job.deadlineTimer) clearTimeout(job.deadlineTimer);
      job.exitCode = code;
      job.signal = signal;
      if (job.status === 'cancelled') {
        job.finishedAt ??= new Date().toISOString();
      } else if (job.status === 'running' || job.status === 'queued') {
        job.status = code === 0 ? 'completed' : 'failed';
      }
      job.finishedAt ??= new Date().toISOString();
      job.process = undefined;
      if (options.onComplete) {
        Promise.resolve()
          .then(() => options.onComplete?.(this.snapshot(job)))
          .then(
            () => { job.completionPending = false; },
            (error: unknown) => {
              job.completionPending = false;
              job.completionError = error instanceof Error ? error.message : String(error);
            }
          );
      }
    });

    return this.snapshot(job);
  }

  /**
   * Register a bounded host task that does not require a child process.
   * Tasks receive an AbortSignal and must check it at safe interruption points.
   */
  public startTask(options: {
    label: string;
    task: (signal: AbortSignal) => Promise<void>;
    timeoutMs?: number;
    onComplete?: (job: JobSnapshot) => void | Promise<void>;
  }): JobSnapshot {
    this.pruneFinishedJobs();
    const job: ManagedJob = {
      jobId: randomUUID(),
      label: options.label,
      status: 'running',
      startedAt: new Date().toISOString(),
      output: '',
      errorOutput: '',
      outputTruncated: false,
      completionPending: Boolean(options.onComplete),
      abortController: new AbortController(),
    };
    this.jobs.set(job.jobId, job);
    const abortSignal = job.abortController?.signal;
    if (!abortSignal) {
      throw new Error('Managed task is missing an abort controller');
    }

    const finish = (status: JobStatus, error?: string): void => {
      if (job.status !== 'running' && job.status !== 'queued') return;
      if (job.deadlineTimer) clearTimeout(job.deadlineTimer);
      job.status = status;
      job.exitCode = status === 'completed' ? 0 : 1;
      job.error = error;
      job.finishedAt = new Date().toISOString();
      if (options.onComplete) {
        Promise.resolve()
          .then(() => options.onComplete?.(this.snapshot(job)))
          .then(
            () => { job.completionPending = false; },
            (completionError: unknown) => {
              job.completionPending = false;
              job.completionError = completionError instanceof Error ? completionError.message : String(completionError);
            }
          );
      }
    };

    if (options.timeoutMs !== undefined && Number.isFinite(options.timeoutMs) && options.timeoutMs > 0) {
      job.deadlineTimer = setTimeout(() => {
        if (job.status !== 'running' && job.status !== 'queued') return;
        job.abortController?.abort();
        job.timedOut = true;
        finish('failed', `Job exceeded timeout of ${options.timeoutMs}ms`);
      }, options.timeoutMs);
    }

    void Promise.resolve()
      .then(() => options.task(abortSignal))
      .then(
        () => finish('completed'),
        (error: unknown) => finish('failed', error instanceof Error ? error.message : String(error))
      );

    return this.snapshot(job);
  }

  public get(jobId: string): JobSnapshot | undefined {
    const job = this.jobs.get(jobId);
    return job ? this.snapshot(job) : undefined;
  }

  public list(): JobSnapshot[] {
    return [...this.jobs.values()]
      .sort((left, right) => right.startedAt.localeCompare(left.startedAt))
      .map(job => this.snapshot(job));
  }

  public cancel(jobId: string): JobSnapshot | undefined {
    const job = this.jobs.get(jobId);
    if (!job) return undefined;
    if (job.status === 'running' || job.status === 'queued') {
      job.status = 'cancelled';
      job.finishedAt = new Date().toISOString();
      if (job.deadlineTimer) clearTimeout(job.deadlineTimer);
      job.abortController?.abort();
      job.process?.kill();
    }
    return this.snapshot(job);
  }

  public async waitForTerminal(jobId: string, timeoutMs = 30000): Promise<{ job?: JobSnapshot; timedOut: boolean }> {
    const boundedTimeout = Number.isFinite(timeoutMs) ? Math.max(1, Math.min(timeoutMs, 600000)) : 30000;
    const startedAt = Date.now();
    while (Date.now() - startedAt < boundedTimeout) {
      const job = this.get(jobId);
      if (!job) return { timedOut: false };
      if (job.status === 'completed' || job.status === 'failed' || job.status === 'cancelled') {
        return { job, timedOut: false };
      }
      await new Promise<void>(resolve => setTimeout(resolve, 25));
    }
    return { job: this.get(jobId), timedOut: true };
  }

  private appendOutput(job: ManagedJob, field: 'output' | 'errorOutput', text: string): void {
    const next = `${job[field]}${text}`;
    if (next.length > MAX_OUTPUT_SIZE) {
      job[field] = next.slice(-MAX_OUTPUT_SIZE);
      job.outputTruncated = true;
    } else {
      job[field] = next;
    }
  }

  private snapshot(job: ManagedJob): JobSnapshot {
    const { process: _process, ...snapshot } = job;
    return { ...snapshot };
  }

  private pruneFinishedJobs(): void {
    if (this.jobs.size < MAX_JOBS) return;
    for (const [jobId, job] of this.jobs) {
      if (job.status !== 'running' && job.status !== 'queued' && job.completionPending !== true) {
        this.jobs.delete(jobId);
      }
      if (this.jobs.size < MAX_JOBS) return;
    }
  }
}

export const jobManager = new JobManager();
