# Plugins/NebulaForgeBridge

Editor-only Unreal Engine 5.0-5.8 Preview plugin. Version source is `NebulaForgeBridge.uplugin` (`VersionName` currently `0.5.30`). It provides the WebSocket automation bridge used by the TypeScript server and an optional native MCP HTTP/SSE endpoint.

## STRUCTURE
```
NebulaForgeBridge/
|-- NebulaForgeBridge.uplugin
|-- Config/                       # plugin defaults and packaging filters
`-- Source/NebulaForgeBridge/
    |-- NebulaForgeBridge.Build.cs
    |-- Public/
    |   |-- NebulaForgeBridgeSettings.h
    |   |-- NebulaForgeBridgeSubsystem.h
    |   `-- McpConnectionManager.h
    `-- Private/
        |-- NebulaForgeBridgeSubsystem.cpp
        |-- NebulaForgeBridge_ProcessRequest.cpp
        |-- McpConnectionManager.cpp / McpBridgeWebSocket.cpp
        |-- MCP/                  # native MCP transport and self-describing tools
        |-- McpSafeOperations.h   # UE 5.7-safe save/load wrappers
        |-- NebulaForgeBridgeHelpers.h
        `-- NebulaForgeBridge_*Handlers.cpp  # 58 domain handler files
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add action handler | `Private/NebulaForgeBridge_*Handlers.cpp` | Keep domain naming aligned with existing files |
| Register handler | `Private/NebulaForgeBridgeSubsystem.cpp` | Add to `InitializeHandlers()` |
| Declare handler | `Private/McpHandlerDeclarations.h` or subsystem header | Match existing declaration location |
| Route requests | `Private/NebulaForgeBridge_ProcessRequest.cpp` | Game-thread dispatch, unsafe-state deferral, reentrancy guard |
| WebSocket bridge | `Private/McpConnectionManager.cpp`, `Private/McpBridgeWebSocket.cpp` | Listen host, ports, token auth, rate limits |
| Native MCP | `Private/MCP/` | See nested AGENTS for `/mcp` transport and tool registry rules |
| Settings | `Public/NebulaForgeBridgeSettings.h`, `Private/NebulaForgeBridgeSettings.cpp` | Loopback, TLS, token, native MCP, debug knobs |
| Packaging | `scripts/package-plugin.*`, `Config/FilterPlugin.ini` | RunUAT package, installed flag, zip output |

## CONVENTIONS
- Handlers run through the subsystem request queue and game-thread dispatch. Do not execute editor API calls from socket threads.
- `InitializeHandlers()` is the authoritative action string map for WebSocket automation requests.
- Defer work while Unreal is saving packages, garbage collecting, or async loading; do not add bypasses around unsafe-state checks.
- Build configuration is intentionally version-aware: keep `Build.cs` feature probes and optional dependency guards when adding engine modules.
- Optional plugin features should fail gracefully when the UE module/plugin is unavailable.

## UE SAFETY
- Use `McpSafeAssetSave`, `McpSafeLevelSave`, and `McpSafeLoadMap` from `McpSafeOperations.h` instead of raw package save/load calls.
- For Blueprint SCS work, create nodes/templates through SCS ownership patterns (`CreateNode`, `AddNode`) instead of assigning arbitrary outers.
- Avoid `ANY_PACKAGE`; use modern lookup helpers or `nullptr`-based lookups.
- Avoid modal asset saves on newly created assets; they can crash editor/D3D12 paths.

## SECURITY
- Default binding is loopback-only. `bAllowNonLoopback` must be explicit before binding LAN addresses.
- Capability-token auth applies to both WebSocket and native MCP when `bRequireCapabilityToken` is enabled.
- Path helpers must reject traversal and absolute host paths before touching the filesystem.
- Message/request rate limits are settings-driven; do not remove enforcement in connection code.

## ANTI-PATTERNS
- Blocking the game thread from socket accept/read/write loops.
- Adding handler actions without TS schema/action coverage and integration tests.
- Hardcoded `C:\`/`X:\` paths or project-local absolute paths in handlers/scripts.
- Editing generated plugin outputs: skip `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, and repo-root `build/` packaging output.
