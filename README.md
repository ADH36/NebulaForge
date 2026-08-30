<div align="center">

# ⚡ NebulaForge

### Give your AI assistant a controllable Unreal Engine

Inspect worlds. Author assets. Build gameplay. Run tests. Profile performance. Ship with confidence.

[**Get Started**](#getting-started) · [**Explore Features**](#features) · [**Production Audit**](docs/production-capability-audit.md) · [**Join Discussions**](https://github.com/ADH36/NebulaForge/discussions)

</div>

<div align="center">

<img src="https://img.shields.io/badge/Unreal%20Engine-5.0--5.8-0E1128?style=for-the-badge&logo=unrealengine" alt="Unreal Engine 5.0 to 5.8">
<img src="https://img.shields.io/badge/MCP-Streamable%20HTTP%20%7C%20stdio-6E56CF?style=for-the-badge" alt="MCP transports">
<img src="https://img.shields.io/badge/TypeScript%20%2B%20C%2B%2B-automation-3178C6?style=for-the-badge" alt="TypeScript and C++">
<img src="https://img.shields.io/badge/Tools-23%20canonical-22C55E?style=for-the-badge" alt="23 canonical tools">

</div>

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![NPM Package](https://img.shields.io/npm/v/unreal-nebula-forge-mcp-server)](https://www.npmjs.com/package/unreal-nebula-forge-mcp-server)
[![MCP SDK](https://img.shields.io/badge/MCP%20SDK-TypeScript-blue)](https://github.com/modelcontextprotocol/sdk)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.8-orange)](https://www.unrealengine.com/)
[![MCP Registry](https://img.shields.io/badge/MCP%20Registry-Published-green)](https://registry.modelcontextprotocol.io/)
[![Project Board](https://img.shields.io/badge/Project-Roadmap-blueviolet?logo=github)](https://github.com/users/ADH36/projects/3)
[![Discussions](https://img.shields.io/badge/Discussions-Join-brightgreen?logo=github)](https://github.com/ADH36/NebulaForge/discussions)

A production-minded Model Context Protocol (MCP) server and Unreal Engine 5 bridge that lets AI assistants inspect, author, test, play, profile, and package Unreal projects. Built with TypeScript and C++.

> **Status:** broad editor automation is available today. Project-specific gameplay architecture, platform certification, external stores, and some engine/plugin-dependent workflows still require live project verification.

<details open>
<summary><strong>✨ Recent capability additions</strong></summary>

- DataTable row modification and deletion with transactional reflected-property validation and optional safe saves.
- Managed asynchronous PCG generation with timeout, cancellation, completion events, and polling.
- Desktop packaging/staging for Windows, Linux, and macOS, plus Android ADB and iOS/tvOS simulator deployment when host prerequisites are available.
- Release gates that combine project/plugin checks, archive and SHA-256 manifest validation, and optional Unreal automation tests.
- Unreal Insights, memory, network-profiler, stat, and Visual Logger capture workflows with bounded job results.
- Media Framework playback, Take Recorder lifecycle, replay controls, online identity/presence status, and controlled network conditions.

</details>

---

## Table of Contents

- [Features](#features)
- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Available Tools](#available-tools)
- [Production Workflows](#production-workflows)
- [Docker](#docker)
- [Documentation](#documentation)
- [Community](#community)
- [Development](#development)
- [Contributing](#contributing)

---

## The short version

NebulaForge turns natural-language intent into validated Unreal Editor operations through one compact MCP surface. Pick the connection that fits your workflow:

<div align="center">

| 🟣 **Native MCP** | 🔵 **TypeScript Bridge** |
|---|---|
| Direct HTTP + SSE from the editor | stdio for Claude Desktop, Cursor, scripts, and CI |
| No Node.js or npm required | Node.js 18+ with WebSocket automation |
| http://127.0.0.1:3000/mcp | node dist/cli.js |
| Best for the fastest local setup | Best for host-side workflows and integrations |

</div>

~~~mermaid
flowchart LR
    A[AI assistant] --> B{Choose a transport}
    B -->|Native MCP| C[HTTP + SSE]
    B -->|TypeScript bridge| D[stdio + WebSocket]
    C --> E[NebulaForge Bridge]
    D --> E
    E --> F[Unreal Editor]
    E --> G[PIE / packaged verification]
    E --> H[Assets, worlds, gameplay, tests, release]
~~~

---

## Features

| Category | Capabilities |
|----------|-------------|
| **Assets & Data** | Browse, inspect, import, duplicate, rename, delete, validate, and author Materials, Data Assets, Primary Assets, DataTables, CurveTables, Media, audio, and physical materials |
| **World Building** | Levels, sublevels, World Partition, HLOD, data layers, landscapes, splines, foliage, water, sky, weather, time of day, procedural geometry, and PCG |
| **Actors & Blueprints** | Spawn and transform actors; edit components, SCS templates, graphs, pins, variables, widgets, bindings, CDOs, materials, cameras, tags, physics, and input |
| **Gameplay & AI** | GAS, character movement, combat, inventory, interaction, Behavior Trees, EQS, State Trees, Smart Objects, perception, navigation, Control Rig, IK Rig, cloth, vehicles, and ragdolls |
| **Editor & Runtime** | PIE, packaged launches, cameras, viewports, screenshots, simulated input, runtime inspection, SaveGame slots, Gameplay Tags, config, and async jobs |
| **VFX, Audio & Cinematics** | Niagara and VFX budgets, Sequencer, Movie Render Queue, Sound Cues, MetaSounds, source effects, media playback, Take Recorder, and replays |
| **Testing & Release** | Automation/functional tests, Blueprint/map/data validation, Unreal Insights, memory/network profiling, UAT packaging, signing, local deployment, release gates, and manifest checks |
| **Online Services** | Session lifecycle, identity, achievements, leaderboards, stats, friends, presence, external UI, and controlled network conditions |

### Architecture

- **Native C++ Automation** — All operations route through the NebulaForge Bridge plugin
- **Dual Transport** — Native HTTP/SSE (no bridge needed) or WebSocket via TypeScript bridge
- **Dynamic Type Discovery** — Runtime introspection for lights, debug shapes, and sequencer tracks
- **Graceful Degradation** — Server starts even without an active Unreal connection
- **On-Demand Connection** — Retries automation handshakes with exponential backoff
- **Command Safety** — Blocks dangerous console commands with pattern-based validation
- **Capability Token Auth** — Optional token-based authentication for both WS and HTTP transports
- **Asset Caching** — 10-second TTL for improved performance
- **Metrics Rate Limiting** — Per-IP rate limiting (60 req/min) on Prometheus endpoint
- **Centralized Configuration** — Unified class aliases and type definitions

---

### Quick mental model

| If you want to... | Start with |
|---|---|
| Connect directly from Claude Code or Cursor | Native MCP at http://127.0.0.1:3000/mcp |
| Use Claude Desktop, scripts, or CI over stdio | TypeScript bridge with Node.js 18+ |
| Discover what this project can support | inspect → production_capabilities |
| Run long work safely | Managed async actions with status, progress, cancellation, and terminal results |
| Add a native Unreal operation | The bridge handler map and plugin extension guide |

## Getting Started

### Prerequisites

- **Unreal Engine** 5.0–5.8 (5.8 preview validated)

Choose your transport:
- **Option A: Native MCP** (recommended) — no additional dependencies
- **Option B: TypeScript Bridge** — requires **Node.js** 18+

### Step 1: Install MCP Server (Option B only — skip for Native MCP)

> Skip this step if using **Option A: Native MCP Transport** ([Step 4A](#option-a-native-mcp-transport-direct-http--no-bridge-needed) below).

**NPX (Recommended):**
```bash
npx unreal-nebula-forge-mcp-server
```

**Clone & Build:**
```bash
git clone https://github.com/ADH36/NebulaForge.git
cd NebulaForge
npm install
npm run build
node dist/cli.js
```

### Step 2: Install Unreal Plugin

The NebulaForge Bridge plugin is included at `NebulaForge/plugins/NebulaForgeBridge`.

#### From source (requires a project with code target)

Your project must have a code target (`.sln` or `.xcworkspace`).
Blueprint-only projects cannot compile native plugins — to convert, add any class via **Tools > New C++ Class** in the editor.

**Method 1: Copy Folder**
```text
Copy:  NebulaForge/plugins/NebulaForgeBridge/
To:    YourUnrealProject/Plugins/NebulaForgeBridge/
```

**Method 2: External Plugin Directory (no copy needed)**
1. Open Unreal Editor → **Edit → Plugins**
2. Click **Plugin Directories** (bottom-left)
3. In **Additional Plugin Directories**, add the path to `NebulaForge/plugins/`
4. Restart the editor — the plugin will be picked up from the external location

This saves the path in your `.uproject` file so the plugin stays linked without copying.

The plugin compiles automatically when you open the project — UE detects the `.uplugin` + `Source/` and runs UnrealBuildTool.

**Video Guide:**

https://github.com/user-attachments/assets/d8b86ebc-4364-48c9-9781-de854bf3ef7d

> ⚠️ **First-Time Project Open:** UE may prompt *"Would you like to rebuild them now?"* — click **Yes**. If instead you see *"Missing Modules — NebulaForgeBridge. Engine modules cannot be compiled at runtime. Please build through your IDE."* — open your project in **Visual Studio** (Win) or **Xcode** (Mac) and build from there. After that, the editor will open normally with the plugin loaded.

#### Pre-built (works with any project, including Blueprint-only)

Build the plugin once, then distribute the compiled binaries — no IDE or compilation needed on the target machine.

**1. Build:**
```bash
# macOS / Linux
./scripts/package-plugin.sh /path/to/UE_5.6

# Windows
scripts\package-plugin.bat C:\Path\To\UE_5.6
```

This produces a zip like `NebulaForgeBridge-v0.5.30-UE5.6-Mac.zip`.

**2. Install:** unzip into `YourProject/Plugins/` and open the project. That's it — no compilation step.

> Note: pre-built binaries are tied to a specific UE version. A build for 5.6 won't work with 5.5, 5.7, or 5.8.

### Step 3: Enable Required Plugins

Enable via **Edit → Plugins**, then restart the editor.

<details>
<summary><b>Core Plugins (Required)</b></summary>

| Plugin | Required For |
|--------|--------------|
| **NebulaForge Bridge** | All automation operations |
| **Editor Scripting Utilities** | Asset/Actor subsystem operations |
| **Niagara** | Visual effects and particle systems |

</details>

<details>
<summary><b>Optional Plugins (Auto-enabled)</b></summary>

| Plugin | Required For |
|--------|--------------|
| **Level Sequence Editor** | `manage_sequence` operations |
| **Control Rig** | `animation_physics` operations |
| **GeometryScripting** | `manage_geometry` operations |
| **Behavior Tree Editor** | `manage_ai` Behavior Tree operations |
| **Niagara Editor** | Niagara authoring |
| **Environment Query Editor** | AI/EQS operations |
| **Gameplay Abilities** | `manage_gas` operations |
| **MetaSound** | `manage_audio` MetaSound authoring |
| **StateTree** | `manage_ai` State Tree operations |
| **Smart Objects** | AI smart object operations |
| **Enhanced Input** | `manage_networking` input mapping operations |
| **Chaos Cloth** | Cloth simulation |
| **Interchange** | Asset import/export |
| **Data Validation** | Data validation |
| **PCG** | `manage_pcg` graph authoring and execution |
| **Procedural Mesh Component** | Procedural geometry |
| **OnlineSubsystem** | Session/networking operations |
| **OnlineSubsystemUtils** | Session/networking operations |

</details>

> 💡 Optional plugins are auto-enabled by the NebulaForge Bridge plugin when needed.

### Step 4: Configure MCP Client

#### Option A: Native MCP Transport (Direct HTTP — no bridge needed)

The plugin includes a built-in MCP Streamable HTTP server. AI clients connect directly to the plugin over HTTP — no TypeScript bridge, no Node.js, no npm.

**Enable in Unreal:**
1. **Edit > Project Settings > Plugins > NebulaForge Bridge**
2. Check **Enable Native MCP**
3. Set port (default: `3000`)
4. Optionally set **Native MCP Instructions** for project-specific guidance
5. Restart the editor

**Configure your MCP client** to use Streamable HTTP transport at:
```
http://localhost:3000/mcp
```

**Claude Code:**
```bash
claude mcp add unreal-engine --transport http http://localhost:3000/mcp
```

Or manually in `~/.claude/settings.json` or project `.mcp.json`:
```json
{
  "mcpServers": {
    "unreal-engine": {
      "type": "url",
      "url": "http://localhost:3000/mcp"
    }
  }
}
```

**Cursor** (`.cursor/mcp.json`):
```json
{
  "mcpServers": {
    "unreal-engine": {
      "url": "http://localhost:3000/mcp"
    }
  }
}
```

**Verify it works:**
- **Status bar** — look for `● MCP :3000 (2)` in the bottom-right of the editor. Green dot = server running, number in parens = active sessions. Click it to open settings.
- **Output Log** — filter by `LogMcpNativeTransport` to see connections, tool calls, and session activity:
  ```
  LogMcpNativeTransport: Native MCP server started on http://localhost:3000/mcp
  LogMcpNativeTransport: MCP session initialized: ... (client: claude-code 2.1.92, active sessions: 1)
  LogMcpNativeTransport: tools/call: inspect (RequestId=...)
  LogMcpNativeTransport: tools/call completed: ... (tool=inspect, success=true)
  ```

Features:
- SSE streaming for real-time progress during long operations
- Multiple concurrent sessions (Cursor + Claude Code + others simultaneously)
- Dynamic tool management — core tools load by default, enable more via `manage_tools`
- Python execution via `execute_python` action (inline code or .py files)
- Capability token authentication — enable in project settings for network security

#### Option B: TypeScript Bridge (stdio — classic setup)

Add to your Claude Desktop / Cursor config file:

**Using Clone/Build:**
```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "node",
      "args": ["path/to/NebulaForge/dist/cli.js"],
      "env": {
        "UE_PROJECT_PATH": "C:/Path/To/YourProject",
        "MCP_AUTOMATION_PORT": "8091"
      }
    }
  }
}
```

**Using NPX:**
```json
{
  "mcpServers": {
    "unreal-engine": {
      "command": "npx",
      "args": ["unreal-nebula-forge-mcp-server"],
      "env": {
        "UE_PROJECT_PATH": "C:/Path/To/YourProject"
      }
    }
  }
}
```

---

## Configuration

### Environment Variables

```env
# Required
UE_PROJECT_PATH="C:/Path/To/YourProject"

# Automation Bridge
MCP_AUTOMATION_HOST=127.0.0.1
MCP_AUTOMATION_PORT=8091

# LAN Access (optional)
# SECURITY: Set to true to allow binding to non-loopback addresses (e.g., 0.0.0.0)
# Only enable if you understand the security implications.
MCP_AUTOMATION_ALLOW_NON_LOOPBACK=false

# Logging
LOG_LEVEL=info  # debug | info | warn | error

# Optional
MCP_CONNECTION_TIMEOUT_MS=5000
MCP_REQUEST_TIMEOUT_MS=120000
ASSET_LIST_TTL_MS=10000

# Optional Prometheus metrics endpoint
# Loopback-only by default. Non-loopback metrics requires both explicit opt-in and a token.
# MCP_METRICS_PORT=9100
# MCP_METRICS_HOST=127.0.0.1
# MCP_METRICS_ALLOW_NON_LOOPBACK=false
# MCP_METRICS_TOKEN=change-me

# Custom content mount points (comma-separated)
# Plugins with CanContainContent register mount points beyond /Game/.
# MCP_ADDITIONAL_PATH_PREFIXES=/ProjectObject/,/ProjectAnimation/
```

### LAN Access Configuration

By default, the automation bridge only binds to loopback addresses (127.0.0.1) for security. To enable access from other machines on your network:

**TypeScript (MCP Server):**
```env
MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true
MCP_AUTOMATION_HOST=0.0.0.0
```

**Unreal Engine Plugin:**
1. Go to **Edit → Project Settings → Plugins → NebulaForge Bridge**
2. Under **Security**, enable **"Allow Non Loopback"**
3. Under **Connection**, set **"Listen Host"** to `0.0.0.0`
4. Restart the editor

⚠️ **Security Warning:** Enabling LAN access exposes the automation bridge to your local network. Only use on trusted networks with appropriate firewall rules. **Enable capability token authentication** (`Require Capability Token` in project settings) to prevent unauthorized access when using LAN mode.

---

## Available Tools

**23 exposed MCP tools** in broad all-tools mode. Related actions live directly on their parent tools so clients load less context without losing capabilities.

<details>
<summary><b>Core Tools</b></summary>

| Tool | Description |
|------|-------------|
| `manage_asset` | Assets, Materials, Render Targets, Behavior Trees |
| `manage_blueprint` | Blueprints, SCS components, graph editing, UMG widgets, layout, bindings, animations |
| `control_actor` | Spawn, delete, transform, physics, tags |
| `control_editor` | PIE, Camera, viewport, screenshots |
| `manage_level` | Load/save, streaming, lighting |
| `system_control` | UBT, Tests, Logs, Project Settings, CVars, Python Execution |
| `inspect` | Object Introspection |
| `manage_tools` | Dynamic tool management (enable/disable at runtime) |

</details>

<details>
<summary><b>World Building</b></summary>

| Tool | Description |
|------|-------------|
| `build_environment` | Landscapes, foliage, procedural terrain, lighting, spline roads/rivers/fences |
| `manage_level_structure` | Levels, sublevels, World Partition, streaming, data layers, HLOD, volumes |
| `manage_geometry` | Procedural mesh creation and editing with Geometry Script |
| `manage_pcg` | PCG graph assets, subgraphs, input/sampler/filter/spawner nodes, pin connections, execution, partition grid size, and node settings |

</details>

<details>
<summary><b>Gameplay Systems</b></summary>

| Tool | Description |
|------|-------------|
| `animation_physics` | Animation BPs, skeletons, sockets, physics assets, cloth, vehicles, ragdolls, Control Rig, IK |
| `manage_effect` | Niagara, particles, debug shapes, GPU simulations |
| `manage_gas` | Gameplay Ability System: abilities, effects, attributes |
| `manage_character` | Character creation, movement, advanced locomotion |
| `manage_combat` | Weapons, projectiles, damage, melee combat |
| `manage_ai` | AI controllers, Behavior Trees, EQS, perception, State Trees, Smart Objects, NavMesh/pathfinding |
| `manage_inventory` | Items, equipment, loot tables, crafting |
| `manage_interaction` | Interactables, destructibles, triggers |

</details>

<details>
<summary><b>Utility</b></summary>

| Tool | Description |
|------|-------------|
| `manage_audio` | Audio Assets, Components, Sound Cues, MetaSounds, Attenuation |
| `manage_sequence` | Sequencer, cinematics, bindings, tracks, playback, keyframes |
| `manage_networking` | Replication, RPCs, network prediction, sessions, split-screen, LAN/voice, game framework, input mappings |

</details>
### Supported Asset Types

The asset workflow also covers Materials, Material Functions, Material Instances, Data Assets, Primary Assets, DataTables, CurveTables, Render Targets, Media Players, Media Sources, Media Textures, Media Playlists, Sound Cues, MetaSounds, Niagara systems and emitters, State Trees, PCG Graphs, Physics Assets, Animation Sequences, Montages, Control Rigs, IK Rigs, and Level Sequences. Availability varies with the Unreal version and enabled project plugins.

Blueprints • Materials • Textures • Static Meshes • Skeletal Meshes • Levels • Sounds • Particles • Niagara Systems • Behavior Trees

---

## Production Workflows

NebulaForge is designed for iterative AI-assisted development, not only one-off editor commands. Common end-to-end workflows include:

~~~mermaid
flowchart TD
    A[Describe a change] --> B[Inspect capabilities and project state]
    B --> C[Author or mutate Unreal content]
    C --> D[Validate and save]
    D --> E{Verify}
    E -->|PIE| F[Run gameplay checks]
    E -->|Tests| G[Run automation tests]
    E -->|Profile| H[Capture Insights data]
    F --> I[Package or stage]
    G --> I
    H --> I
    I --> J[Release gate]
~~~

1. **Build a world:** create a level, configure World Partition or streaming, author landscape/foliage/water, generate PCG content, then inspect and save the result.
2. **Author gameplay:** create Blueprints and components, wire graph nodes and pins, configure input, add GAS/combat/inventory/interaction systems, compile, and validate.
3. **Test a change:** launch PIE or a packaged build, simulate input, capture screenshots, run automation tests, inspect logs, and poll managed jobs to completion.
4. **Profile a build:** start an Unreal trace, capture Insights/network/memory/Visual Logger data, run bounded analysis, and retain the structured terminal result.
5. **Prepare a release:** validate project plugins and manifests, run UAT with controlled packaging options, stage or deploy locally, then use release_gate to combine artifact, project, and test checks.
6. **Operate online features:** inspect provider capabilities and identity, manage sessions, and apply bounded network conditions for reconnect/soak scenarios.

For the exact support boundary—including engine/plugin prerequisites and known gaps—see the [production capability audit](docs/production-capability-audit.md).

---

## Docker

```bash
docker build -t unreal-mcp .
docker run -it --rm -e UE_PROJECT_PATH=/project unreal-mcp
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [Handler Mappings](docs/handler-mapping.md) | TypeScript to C++ routing |
| [Plugin Extension](docs/editor-plugin-extension.md) | C++ plugin architecture |
| [Testing Guide](docs/testing-guide.md) | How to run and write tests |
| [Roadmap](docs/Roadmap.md) | Development phases |
| [Production Capability Audit](docs/production-capability-audit.md) | Implemented features, prerequisites, and known gaps |
| [UE 5.8 Compatibility Matrix](docs/ue5.8-compatibility-matrix.md) | Engine-version compatibility notes |
| [Native Automation Progress](docs/native-automation-progress.md) | Native MCP transport and tool parity status |
| [Changelog](CHANGELOG.md) | Release history |


---

## Development

```bash
npm install
npm run build:core       # Compile TypeScript
npm run type-check       # Type-check without emitting
npm run lint             # Run ESLint
npm run test:unit        # Vitest unit tests
npm run test:smoke       # Mock-mode stdio smoke test
npm run test:native-parity
npm run test:params      # Static parameter-combination audit
npm test                 # Unreal-dependent integration suite
```

---

## Community

| Resource | Description |
|----------|-------------|
| [Project Roadmap](https://github.com/users/ADH36/projects/3) | Track development progress across 48 phases |
| [Discussions](https://github.com/ADH36/NebulaForge/discussions) | Ask questions, share ideas, get help |
| [Issues](https://github.com/ADH36/NebulaForge/issues) | Report bugs and request features |

---

## Contributing

Contributions welcome! Please:
- Include reproduction steps for bugs
- Keep PRs focused and small
- Follow existing code style

---

## License

MIT — See [LICENSE](LICENSE)
