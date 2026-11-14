## Graphify

Use the project Graphify skill at .codex/skills/graphify/SKILL.md for codebase navigation. The local AST graph is at graphify-out/graph.json. Query it for relevant symbols and verify findings in source when needed. Rebuild the graph after source changes.

On this workstation, run from the repository root in PowerShell:

- Query: & "C:/Users/drknt/.codex/.chatgpt-projects/g-p-68b4c9cb923c81919b33204d3b74f883/graphify-env/Scripts/graphify.exe" query "<question>"
- Update: & "C:/Users/drknt/.codex/.chatgpt-projects/g-p-68b4c9cb923c81919b33204d3b74f883/graphify-env/Scripts/graphify.exe" extract . --code-only --max-workers 4
