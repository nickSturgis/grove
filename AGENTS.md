# AGENTS.md

Single-file bash tool: `cc` launches Claude Code in a per-task git worktree
inside a detached tmux session. No build, no deps beyond `git`, `tmux`,
optionally `fzf`.

## Rules

- Everything lives in `cc`. Keep it one file, POSIX-ish bash, `set -euo pipefail`.
- `cc --state <s>` is a hot path (runs as a Claude Code hook on every prompt).
  Keep it at the top of the file, before any `git`/`tmux` discovery, and never
  let it fail — hooks that error are user-visible noise.
- Worktrees go under `$CC_WORKTREE_ROOT`, never inside the repo.
- All human output goes to **stderr**; stdout is reserved for machine-readable
  rows (`cc --_rows`, consumed by the fzf reload binding).
- Rows are tab-delimited `display\tsession\tpath`. `display` must not contain tabs.

## Testing

No test suite. Verify by hand in a scratch repo:

```bash
export CC_WORKTREE_ROOT=/tmp/ccwt CC_CLAUDE=/bin/true
cd /tmp/scratch-repo && cc -l && cc -n && cc -k wt1
```

Check both picker paths (`CC_PICKER=fzf` and `CC_PICKER=plain`) — they are
separate code paths and only one gets exercised on any given machine.
