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
}

interface ManagedJob extends JobSnapshot {
  process?: ChildProcessWithoutNullStreams;
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
      process: options.process,
    };
    this.jobs.set(job.jobId, job);

    options.process.stdout.on('data', (chunk: Buffer | string) => {
      this.appendOutput(job, 'output', chunk.toString());
    });
    options.process.stderr.on('data', (chunk: Buffer | string) => {
      this.appendOutput(job, 'errorOutput', chunk.toString());
    });
    options.process.once('error', (error: Error) => {
      job.status = 'failed';
      job.error = error.message;
      job.finishedAt = new Date().toISOString();
    });
    options.process.once('close', (code: number | null, signal: NodeJS.Signals | null) => {
      job.exitCode = code;
      job.signal = signal;
      if (job.status === 'cancelled') {
        job.finishedAt ??= new Date().toISOString();
      } else {
        job.status = code === 0 ? 'completed' : 'failed';
        job.finishedAt = new Date().toISOString();
      }
      job.process = undefined;
    });

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
      job.process?.kill();
    }
    return this.snapshot(job);
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
      if (job.status !== 'running' && job.status !== 'queued') {
        this.jobs.delete(jobId);
      }
      if (this.jobs.size < MAX_JOBS) return;
    }
  }
}

export const jobManager = new JobManager();
