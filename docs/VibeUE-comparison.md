# NebulaForge and VibeUE comparison

This is the implementation inventory for the VibeUE comparison. It is based on the VibeUE repository at the comparison revision and NebulaForge's current TypeScript/native surfaces.

## Short version

VibeUE is a UE 5.8 editor-side service and skill layer. NebulaForge is the MCP transport, safety, registry, and broader automation layer. They are complementary: NebulaForge already covers many of VibeUE's domains directly, while the optional VibeUE gateway exposes the VibeUE-specific service depth without duplicating 31 service implementations in this repository.

## Capability matrix

| VibeUE area | NebulaForge status | How to use it |
| --- | --- | --- |
| Actor, asset, blueprint, graph, widget, level, material, Niagara, animation, skeleton, audio, input, gameplay tags, foliage, landscape, PCG, state tree, behavior tree, viewport, profiling | Native or existing consolidated equivalents | Use the matching consolidated tool and action enum. |
| VibeUE's 31 Python service classes and UE 5.8 ToolsetRegistry/AICallable metadata | Optional bridged parity | Install VibeUE into the target project, then use `list_vibeue_services` and `call_vibeue_service`. |
| Landscape material authoring, terrain data, map blockout, UV mapping, RVT, MetaSound, SoundCue | Native coverage where available plus VibeUE gateway for service-specific depth | Use the domain tool first; call the VibeUE service for operations that are specific to its implementation. |
| Performance reports, frame timing, hitch generation, standalone/PIE control, trace regions/bookmarks | Native consolidated performance actions | Use `manage_performance`; Unreal Insights tracing is handled by the existing safe bridge. |
| VibeUE skill packs | Lazy-loaded local skill registry | Use `list_skills` and `get_skills`; installed `Plugins/VibeUE/Content/Skills` is auto-discovered. |
| Deep research and VibeUE terrain web API | Node/stdio-only host utilities | Use `deep_research` and `terrain_data`; these are intentionally not native MCP tools because they require outbound HTTP or host filesystem access. |
| Agent instruction/config generation | Node/stdio-only utility | Use `generate_agent_config` for Codex, Cursor, Claude, Gemini, Hermes, or Copilot. |
| VibeUE's complete editor runtime | Requires a UE 5.8 editor and installed plugin | Run `npm run vibeue:sync` followed by `npm run vibeue:verify`, rebuild the editor, then exercise the service gateway. |

## What was missing and is now covered

- VibeUE service discovery and invocation through a validated generic gateway.
- Performance and tracing actions that had no consolidated parity mapping.
- Lazy skill discovery with VibeUE project-plugin roots.
- Deep research, terrain-data requests, and agent-config generation on the host side.
- A safe installer and preflight verifier for adding VibeUE to an Unreal project.
- A native/stdio parity audit that explicitly reports intentional host-only tools.

## Remaining boundary

The repository does not vendor VibeUE or reimplement its UE-specific service classes. The gateway is the compatibility boundary, which keeps VibeUE updates independent and avoids forking its editor implementation. A full native validation still requires an installed UE 5.8 editor, a target `.uproject`, both plugins enabled, and a live bridge session.

## Integration commands

```bash
npm run vibeue:sync -- --repo https://github.com/kevinpbuckley/VibeUE.git --project /path/to/YourProject
npm run vibeue:verify -- --project /path/to/YourProject
```
