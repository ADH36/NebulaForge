import { describe, expect, it } from 'vitest';
import { automationMessageSchema } from './message-schema.js';

describe('automationMessageSchema', () => {
    it('preserves unknown top-level bridge payload fields', () => {
        const message = {
            type: 'bridge_ack',
            serverName: 'UnrealMCP',
            heartbeatIntervalMs: 15000,
            futureCapability: {
                modes: ['native', 'bridge']
            }
        };

        expect(automationMessageSchema.parse(message)).toEqual(message);
    });

    it('accepts explicit editor and packaged-runtime capability metadata', () => {
        const message = {
            type: 'bridge_ack',
            executionMode: 'editor',
            editorAutomation: true,
            pieRuntimeWorld: true,
            packagedRuntimeAuthoring: false
        };

        expect(automationMessageSchema.parse(message)).toEqual(message);
    });

    it('accepts the opt-in packaged runtime capability contract', () => {
        const message = {
            type: 'bridge_ack',
            serverName: 'NebulaForgeRuntime',
            executionMode: 'runtime',
            editorAutomation: false,
            pieRuntimeWorld: true,
            packagedRuntimeAuthoring: false,
            capabilities: ['runtime_health', 'get_runtime_capabilities', 'get_runtime_world']
        };

        expect(automationMessageSchema.parse(message)).toEqual(message);
    });

    it('rejects negative bridge heartbeat intervals', () => {
        expect(() => automationMessageSchema.parse({
            type: 'bridge_ack',
            heartbeatIntervalMs: -1
        })).toThrow();
    });

    it('rejects fractional protocol versions', () => {
        expect(() => automationMessageSchema.parse({
            type: 'bridge_ack',
            protocolVersion: 1.5
        })).toThrow();
    });
});
