# Handler extension

NebulaForge supports project-local native handler aliases through `Config/NebulaForgeHandlers.json`.

```json
{
  "aliases": [
    { "action": "inspect_runtime_world", "target": "get_runtime_world" }
  ]
}
```

Aliases are validated during bridge initialization, must use a new alphanumeric/underscore action name, and can only target an already-registered native handler. The loader does not execute code from JSON.

For a real custom C++ handler, an Unreal module can call the public `UNebulaForgeBridgeSubsystem::RegisterHandler` API during its module or subsystem initialization and provide an `FAutomationHandler` delegate. The delegate receives the request ID, action, JSON payload, and bridge socket, so it can use the same response helpers and security boundaries as built-in handlers.
