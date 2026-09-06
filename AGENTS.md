# AGENTS.md

Single-file bash tool: `grove` launches Claude Code in a per-task git worktree
inside a detached tmux session. No build, no deps beyond `git`, `tmux`,
optionally `fzf`.

## Rules

- Everything lives in `grove`. Keep it one file, POSIX-ish bash, `set -euo pipefail`.
- `grove --state <s>` is a hot path (runs as a Claude Code hook on every prompt).
  Keep it at the top of the file, before any `git`/`tmux` discovery, and never
  let it fail — hooks that error are user-visible noise.
- `grove -h` prints lines 2-13 of the file itself. Edits to the header comment
  must preserve that line range.
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
- Rows are tab-delimited `display\tsession\tpath`. `display` must not contain tabs,
  but does carry ANSI colour — fzf gets `--ansi`, the plain picker prints it raw.
  Colour is off unless stdout is a tty; fzf's reload binding runs `rows` down a pipe,
  so it passes `GROVE_ANSI=1` to opt back in. `NO_COLOR` and `TERM=dumb` disable it.
- The picker's last row is synthetic: `new_row` carries `%new%` in its path field and
  every picker resolves that to `new_worktree`. `--_rows` appends it (fzf reload must
  keep it); `grove -l` calls `rows` directly and never shows it.

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

## Testing

No test suite. Verify by hand in a scratch repo:

```bash
export GROVE_CLAUDE=/bin/true
cd /tmp/scratch-repo && grove -l && grove -n && grove -k wt1
git status --porcelain          # must be empty: the worktree is excluded

# and the override path, which is a separate branch of the base-dir logic
GROVE_ROOT=/tmp/grovetest grove -n && grove -l && grove -k wt2
```

Prompts read from `/dev/tty`, so testing the confirm paths needs a pty:
`printf 'y\n' | script -qec 'grove -k wt1' /dev/null`.

Check both picker paths (`GROVE_PICKER=fzf` and `GROVE_PICKER=plain`) — they are
separate code paths and only one gets exercised on any given machine. Both are now
cursor UIs, so driving them takes a real terminal: run grove in a detached tmux
session, `send-keys -t =<session>:` the arrows, then `capture-pane` to see the frame.
The plain picker needs stderr to be a tty — redirect it and it falls back to the
numbered prompt, which is also the `TERM=dumb` path.


## Keeping these files navigable

An `AGENTS.md` orients and routes. It is **not** where a subsystem's reasoning accumulates — that's
what a companion doc is for (`git-workflow.md`, `docs/specs/`).

**Budget: 12,000 characters / ~150 lines per `AGENTS.md`.** Check it whenever you change one:

```sh
find . -name .venv -prune -o -name worktrees -prune -o -name AGENTS.md -print | xargs wc -lc |
  awk '$3 != "total" && ($1 > 150 || $2 > 12000) { print "OVER:", $3, $1"L", $2"c" }'
```

Over budget means the node has stopped routing and started explaining. Fix it by pushing detail
down — into a child `AGENTS.md`, a companion doc, or the code — **never by trimming facts.** Same
rule at every level: **a fact lives in exactly one place, and everywhere else links to it.** The
method for doing that — and for auditing the whole trail — is the `breadcrumb-cleanup` skill.

## Elsewhere

- **Issues are Gitea issues** on `gitea.glowlab.cc` (`glowlab/grove`), driven via
  `mcp__gitea__*` — there is no `tea` CLI. Cite them inline as `Gitea **#N**`. 