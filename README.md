# cc

Launch Claude Code in an isolated git worktree, inside a tmux session that
outlives your terminal. One bash file.

```
cc                pick a worktree, or make one if the repo has none
cc -n             always make a fresh worktree
cc -l             list this repo's cc worktrees and their state
cc -k <slug>      kill the session and remove the worktree
cc --hooks        print the Claude Code hook config for live status
cc -- <args>      pass args through to claude (e.g. cc -- --continue)
```

Each worktree gets its own branch (`cc/<slug>`) and its own tmux session
(`cc-<repo>-<slug>`), so parallel Claude sessions never touch each other's
files. Worktrees live under `$CC_WORKTREE_ROOT` (default
`~/.cc-worktrees/<repo>/<slug>`), outside your checkout.

## Install

Drop `cc` on your `$PATH`, then merge `cc --hooks` output into
`~/.claude/settings.json`. The hooks stamp each session's state onto its tmux
session, so the picker can sort by what needs you:

```
  wt3         needs you             cc/wt3 ~2
  wt1         done (attached)       cc/wt1
  wt2         working               cc/wt2 ~7
```

Needs `git` and `tmux`; uses `fzf` for the picker (with a live pane preview) if
it's installed, otherwise a numbered prompt.

## Config

| Env | Default | |
|---|---|---|
| `CC_WORKTREE_ROOT` | `~/.cc-worktrees` | where worktrees go |
| `CC_CLAUDE` | `claude` | the binary to launch |
| `CC_PICKER` | `auto` | `auto`, `fzf`, or `plain` |

## Credit

Started as our own worktree-per-task tmux launcher; the session-state and
picker ideas were sharpened by [craftzdog/tmux-claude-session-manager](https://github.com/craftzdog/tmux-claude-session-manager).
