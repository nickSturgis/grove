# Issue tracker: Gitea

Issues and specs live as Gitea issues on `gitea.glowlab.cc`, repo `glowlab/grove`.
Driven via the `mcp__gitea__*` MCP tools — **there is no `tea` CLI, and `gh` does not
apply.** Every call takes `owner: "glowlab"`, `repo: "grove"`.

Cite issues inline as `Gitea **#N**`.

## Conventions

- **Create**: `issue_write` `method: "create"` + `title`, `body`.
- **Read**: `issue_read` `method: "get"` — plus `"get_comments"` and `"get_labels"`,
  which are *separate calls*; `get` alone returns neither.
- **List**: `list_issues` with `state` ("open"/"closed"/"all", default "all") and
  `labels` (an array of label **names** — the one place names are accepted).
- **Comment**: `issue_write` `method: "add_comment"` + `body`.
- **Close**: comment first, then `issue_write` `method: "update"`, `state: "closed"`.
- **Cross-repo search**: `search_issues` with `query`, `owner`, `type: "issues"`.

## Labels are IDs, not names

`issue_write`'s `labels` field takes **numeric label IDs**. Passing the strings from
`docs/agents/triage-labels.md` will not work. Resolve first:

1. `label_read` `method: "list_repo_labels"` → the `id` for each name.
2. Missing? `label_write` `method: "create_repo_label"` + `name`, `color` (hex
   `#RRGGBB`, **required**), `description`.
3. Then `issue_write` `method: "add_labels"` / `"replace_labels"` with those ids.

Removal is one label at a time: `method: "remove_label"` + `label_id`.

## PRs as a request surface

**No.** _(Set to `yes` if external PRs should enter the triage queue; `/triage` reads
this flag.)_ PR tools are `mcp__gitea__pull_request_read` / `_write` / `list_pull_requests`.
Gitea shares one number space across issues and PRs, so a bare `#42` may be either.

## When a skill says "publish to the issue tracker"

Create a Gitea issue.

## When a skill says "fetch the relevant ticket"

`issue_read` `get`, then `get_comments`.

## Wayfinding operations

Used by `/wayfinder`. The **map** is one issue; **children** are tickets.

- **Map**: an issue labelled `wayfinder:map`, holding Notes / Decisions-so-far / Fog.
- **Child ticket**: an issue labelled `wayfinder:<type>` (`research`/`prototype`/
  `grilling`/`task`). The MCP server exposes **no sub-issue tool**, so link by
  convention: `Part of #<map>` as the first line of the child body, and a task-list
  entry in the map body.
- **Blocking**: the MCP server exposes **no dependency tool** either (Gitea has the
  feature; it is not surfaced here). Use a `Blocked by: #<n>, #<n>` line at the top of
  the child body. A ticket is unblocked when every listed blocker is closed — check
  with `issue_read` `get` on each.
- **Frontier query**: `list_issues` `state: "open"` over the map's children; drop any
  with an unclosed `Blocked by` entry or an assignee; first in map order wins.
- **Claim**: `issue_write` `method: "update"` + `assignees: ["derglow"]`. Note this
  *replaces* the assignee list.
- **Resolve**: `add_comment` with the answer, `update` to `state: "closed"`, then
  append a context pointer to the map's Decisions-so-far.
