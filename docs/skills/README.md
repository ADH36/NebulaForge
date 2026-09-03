# NebulaForge workflow skills

Use the MCP `list_skills` action to discover installed workflow packs and `get_skills` to load only
the packs needed for the current task. Packs under `Content/Skills` are discovered automatically
and take precedence over duplicate local names.

The Node/stdio surface also provides host-oriented utilities (`deep_research`, `terrain_data`,
skill loading, and agent-config generation). Unreal-native MCP clients use the NebulaForge service
consolidated domain tools for editor-side capabilities.
