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
- Rows are tab-delimited `display\tsession\tpath`. `display` must not contain tabs.

## Gotchas found the hard way

- fzf substitutes `{2}` as a *shell-quoted* string, so it must never sit inside
  quotes: `-t ={2}:` works, `-t "={2}:"` passes literal quote characters.
- `git rev-list --exclude=<glob>` globs are relative to the ref-listing option
  that follows, so it's `--exclude=grove/wt1 --branches`, not
  `--exclude=refs/heads/grove/wt1`. And `--all` silently includes every
  worktree's HEAD, which would always protect the branch you're testing.
- `a | b || c` binds `||` to `b`. Wrap the fallback: `{ a || c; } | b`.

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
separate code paths and only one gets exercised on any given machine.
