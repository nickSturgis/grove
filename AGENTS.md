# AGENTS.md

Single-file bash tool: `grove` launches Claude Code in a per-task git worktree
inside a detached tmux session. No build, no deps beyond `git`, `tmux`,
optionally `fzf`.

## Rules

- Everything lives in `grove`. Keep it one file, POSIX-ish bash, `set -euo pipefail`.
- `grove --state <s>` is a hot path (runs as a Claude Code hook on every prompt).
  Keep it at the top of the file, before any `git`/`tmux` discovery, and never
  let it fail — hooks that error are user-visible noise.
- `grove -h` reprints the file's own leading comment block — every `#` line from
  line 2 until the first line that isn't one. Keep that block the whole help text.
- grove runs **outside a git repo too**: there it has no worktrees, only the
  sessions tab. Anything needing `$main_root`/`$wt_base` must sit behind
  `need_repo` or an `$in_repo` test.
- Worktrees go at `<repo>/.claude/worktrees/<slug>` — Claude Code's own
  location, so `/resume` finds their sessions. Setting `$GROVE_ROOT` restores
  the old out-of-repo layout. Sessions are `grove-<repo>-<slug>`.
- The **`grove/<slug>` branch prefix, not the path, is what marks a worktree as
  grove's.** That directory is shared with `claude -w` worktrees (branch
  `worktree-<name>`), which grove must never treat as its own, and the branch test
  also keeps finding worktrees left over from the old `$GROVE_ROOT` layout. A
  grove worktree on a detached HEAD drops out of the listing; that's accepted.
- **Adoption is that rename and nothing else** — `adopt_worktree` moves the branch
  to `grove/<slug>`, after which the worktree is ordinary and no other code path
  knows the difference. So it must **rename, never create**: a surviving
  `worktree-<name>` still holds the commits, and `orphan_count` would report zero
  and let `-k` delete them unasked. Same reason the slug is forced to the
  directory basename. `foreign_worktrees` lists candidates as `unadopted` and
  every picker adopts on open, so the scope is `$wt_base` only — a checkout parked
  elsewhere in the repo must never be one keystroke from a rename. `grove adopt`
  takes any path, and resolves it to its worktree **toplevel**: `.` reaches the
  main checkout, and renaming from there renames main. It did, once.
- A session pins the claude binary it launched, so it goes stale as Claude Code
  updates under it. `@grove_claude` stamps that binary at launch and `stale`
  compares; an **unstamped** session (started by an older grove) is never stale.
  The restart is a `respawn-pane`, never kill + new-session — the session stays
  up, so it cannot trip the `exit-empty` race below — and it fires only when the
  cost is provably nil: idle per the `Stop` hook — or never prompted, which the
  hooks report as no state at all — and nobody attached. Anything else just
  wears the `↑<version>` marker. See `restartable` for why.
- Opening a worktree with **no live session** fast-forwards it onto main first:
  `maybe_sync`, called only from `launch`'s new-session branch, and only when the
  worktree is clean (untracked counts) and has no commits main lacks. "Main" is
  whatever the main checkout has out — what `new_worktree` branches from — and
  `base_ref` resolves it per path, because sessions-tab rows span repos. Listings
  mark the gap as `⇡own⇣behind`; double arrows, because `↑` is the claude marker.
- Never destroy work without confirming: `-k` checks both uncommitted files and
  commits reachable from no other ref. `-f` answers that prompt up front (the
  warning still prints), and is the only way to use `-k` without a tty. It does
  **not** override a **live** Claude Code worktree lock — that names a process
  editing those files right now, and `-f` typed for the common **stale** lock is
  not consent to corrupt it. A stale one clears with no flag at all: see
  `lock_reason`/`lock_live`.
- All human output goes to **stderr**; stdout is reserved for machine-readable
  rows (`grove --_rows`, consumed by the fzf reload binding).
- Rows are tab-delimited `display\tsession\tpath\ttab`. `display` must not contain tabs,
  but does carry ANSI colour — fzf gets `--ansi`, the plain picker prints it raw.
  Colour is off unless stdout is a tty; fzf's reload binding runs `rows` down a pipe,
  so it passes `GROVE_ANSI=1` to opt back in. `NO_COLOR` and `TERM=dumb` disable it.
- The picker's last row is synthetic: `new_row` carries `%new%` in its path field and
  every picker resolves that to `new_worktree`. `--_rows` appends it (fzf reload must
  keep it); `grove -l` calls `rows` directly and never shows it.
- Two tabs — `rows` (this repo's worktrees) and `session_rows` (every live
  `grove-*` tmux session, any repo). Because there are exactly two, ← and → are
  **absolute, not a toggle**: ← is always worktrees, → always sessions, so no
  picker has to track which tab it is on. The 4th row field carries the tab name
  purely so the kill key can reload the tab you were on (`--_rows {4}`).
- `--_rows` is fzf's alone, and prints the tab bar as its **first line**, pinned
  with `--header-lines=1`. The plain picker and `-l`/`-s` call `rows`/
  `session_rows` directly and never see it.
- `open_row` is how every picker acts on a choice: `%new%` makes a worktree, a
  live session is attached **by name** (a sessions-tab row belongs to another
  repo, where `launch` would compute the wrong session name), else `launch`.
- Killing a session is not destroying work — the worktree stays — so the kill
  key needs no confirmation. It routes through `grove --_kill`, which refuses
  any name that isn't `grove-*` and refuses the session it is running in.

## Gotchas found the hard way

- fzf substitutes `{2}` as a *shell-quoted* string, so it must never sit inside
  quotes: `-t ={2}:` works, `-t "={2}:"` passes literal quote characters.
- `git rev-list --exclude=<glob>` globs are relative to the ref-listing option
  that follows, so it's `--exclude=grove/wt1 --branches`, not
  `--exclude=refs/heads/grove/wt1`. And `--all` silently includes every
  worktree's HEAD, which would always protect the branch you're testing.
- `a | b || c` binds `||` to `b`. Wrap the fallback: `{ a || c; } | b`.
- `printf '%-10s'` counts ANSI escape bytes as width, so pad the plain string and
  wrap the padded result in colour — never colour first.
- tmux `send-keys`/`capture-pane`/`respawn-pane`/`set-option` take a *pane* target,
  so `-t =<session>` fails with "can't find pane" where `kill-session` accepts it.
  The trailing colon is required — and on `set-option -q` the failure is silent,
  so the option simply never gets set.
- The plain picker marks the current row with a pointer, not reverse video: each row
  carries its own colour resets, which would cancel a reverse attribute mid-line.
- `exit-empty` (on by default) stops the tmux **server** the moment its last
  session dies, and a server mid-shutdown still accepts `new-session` before
  taking that session down with it. `launch` therefore `has-session`-checks
  before attaching: without it the client prints a bare `[exited]`, which reads
  like grove crashed when nothing of grove's went wrong.
- fzf `change-header` needs 0.42; Debian still ships **0.38**, which dies with
  "unknown action" on an unknown binding — taking the whole picker with it. That
  is why the tab bar rides in on `--header-lines`. Check any new binding against
  0.38 before using it.
- The plain picker's frame changes height when you switch tabs, so it rewinds by
  the count it actually drew (`FRAME`) and blanks the surplus lines — rewinding
  by row count alone leaves the old tab's rows stranded below.

## Testing

No test suite — grove is verified by hand in scratch repos. The procedure, the
states you have to force, and the rules for isolating and tearing down the test's
tmux server are in `docs/testing.md`.


## Keeping these files navigable

An `AGENTS.md` orients and routes. It is **not** where a subsystem's reasoning accumulates — that's
what a companion doc is for (`docs/specs/`).

**Budget: 12,000 characters / ~150 lines per `AGENTS.md`.** Check it whenever you change one:

```sh
find . -name .venv -prune -o -name worktrees -prune -o -name AGENTS.md -print | xargs wc -lc |
  awk '$3 != "total" && ($1 > 150 || $2 > 12000) { print "OVER:", $3, $1"L", $2"c" }'
```

Over budget means the node has stopped routing and started explaining. Fix it by pushing detail
down — into a child `AGENTS.md`, a companion doc, or the code — **never by trimming facts.** Same
rule at every level: **a fact lives in exactly one place, and everywhere else links to it.** The
method for doing that — and for auditing the whole trail — is the `breadcrumb-cleanup` skill.

## Agent skills

Config the `mattpocock-skills` engineering skills read:

- **Issue tracker** — Gitea, via `mcp__gitea__*`: `docs/agents/issue-tracker.md`
- **Triage labels** — five canonical roles, label string = role name:
  `docs/agents/triage-labels.md`
- **Domain docs** — single-context, `CONTEXT.md` + `docs/adr/`: `docs/agents/domain.md`

## Elsewhere

- **Issues are Gitea issues.** Repo, tools and conventions are canonical in
  `docs/agents/issue-tracker.md`; never restated here.
- **The workflow that lands this repo's work** — branching, committing, merging
  to `main` — is the `git-workflow@glow` skill
  (`~/glow-marketplace/plugins/git-workflow/skills/git-workflow/SKILL.md`).
  Canonical there; never restated here. 