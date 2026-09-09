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
  `worktree-<name>`), which grove must never list or delete, and the branch test
  also keeps finding worktrees left over from the old `$GROVE_ROOT` layout. A
  grove worktree on a detached HEAD drops out of the listing; that's accepted.
- Never destroy work without confirming: `-k` checks both uncommitted files and
  commits reachable from no other ref.
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
- tmux `send-keys`/`capture-pane` take a *pane* target, so `-t =<session>` fails with
  "can't find pane" where `kill-session` accepts it. The trailing colon is required.
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

No test suite. Verify by hand in a scratch repo:

```bash
export GROVE_CLAUDE=/bin/true
cd /tmp/scratch-repo && grove -l && grove -n && grove -k wt1
git status --porcelain          # must be empty: the worktree is excluded

# and the override path, which is a separate branch of the base-dir logic
GROVE_ROOT=/tmp/grovetest grove -n && grove -l && grove -k wt2
```

The sessions tab needs **two** scratch repos, or it never proves it resolves the
right session for a row outside the current one. `grove -s` from `~` is the
no-repo path. Force the interesting states by hand — `tmux set-option -t
<session> @grove_state waiting`, and `git worktree remove --force` a live
session's directory to get the `gone` row.

Prompts read from `/dev/tty`, so testing the confirm paths needs a pty:
`printf 'y\n' | script -qec 'grove -k wt1' /dev/null`.

Check both picker paths (`GROVE_PICKER=fzf` and `GROVE_PICKER=plain`) — they are
separate code paths and only one gets exercised on any given machine. Both are now
cursor UIs, so driving them takes a real terminal: run grove in a detached tmux
session, `send-keys -t =<session>:` the arrows, then `capture-pane` to see the frame.
The plain picker needs stderr to be a tty — redirect it and it falls back to the
numbered prompt, which is also the `TERM=dumb` path.

**Tear down by name, never by sweep.** Test sessions share one tmux server with
the owner's real ones, so filter every cleanup to the sessions the test made:

```bash
tmux list-sessions -F '#{session_name}' | grep -E '^(grove-sr|driver|pick)' |
  while read -r s; do tmux kill-session -t "=$s"; done
```

A bare `for s in $(tmux list-sessions -F '#{session_name}')` has already killed a
live working session once — and because it took the last session with it, the
server went too, so the owner's next `grove` raced a shutting-down server.


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

### Issue tracker

Gitea issues on `gitea.glowlab.cc` (`glowlab/grove`), via `mcp__gitea__*`.
See `docs/agents/issue-tracker.md`.

### Triage labels

The five canonical roles, each label string equal to its name.
See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `CONTEXT.md` + `docs/adr/` at the root. See `docs/agents/domain.md`.

## Elsewhere

- **Issues are Gitea issues.** Repo, tools and conventions are canonical in
  `docs/agents/issue-tracker.md`; never restated here.
- **The workflow that lands this repo's work** — branching, committing, merging
  to `main` — is the `git-workflow@glow` skill
  (`~/glow-marketplace/plugins/git-workflow/skills/git-workflow/SKILL.md`).
  Canonical there; never restated here. 