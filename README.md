# grove

Launch Claude Code in an isolated git worktree, inside a tmux session that
outlives your terminal. One bash file.

A grove is a stand of trees — here, the set of worktrees you have Claude
working in. Plant one per task, survey them, cut them down when merged.

```
grove             pick a worktree, or make one if the repo has none
grove -n          always make a fresh worktree
grove -l          list this repo's worktrees and their state
grove -k <slug>   kill the session and remove the worktree
grove --hooks     print the Claude Code hook config for live status
grove -- <args>   pass args through to claude (e.g. grove -- --continue)
```

Each worktree gets its own branch (`grove/<slug>`) and its own tmux session
(`grove-<repo>-<slug>`), so parallel Claude sessions never touch each other's
files. Worktrees live under `$GROVE_ROOT` (default `~/.grove/<repo>/<slug>`),
outside your checkout.

`grove -k` refuses to silently discard work: it warns and shows the commits if
the branch holds anything not merged anywhere else.

## Install

Symlink `grove` onto your `$PATH`, then merge `grove --hooks` output into
`~/.claude/settings.json`:

```bash
ln -s "$PWD/grove" ~/.local/bin/grove
```

The hooks stamp each session's state onto its tmux session, so the picker can
sort by what needs you:

```
  wt3         needs you             grove/wt3 ~2
  wt1         done (attached)       grove/wt1
  wt2         working               grove/wt2 ~7
```

Needs `git` and `tmux`; uses `fzf` for the picker (with a live pane preview) if
it's installed, otherwise a numbered prompt.

## Config

| Env | Default | |
|---|---|---|
| `GROVE_ROOT` | `~/.grove` | where worktrees go |
| `GROVE_CLAUDE` | `claude` | the binary to launch |
| `GROVE_PICKER` | `auto` | `auto`, `fzf`, or `plain` |

## Credit

Started as our own worktree-per-task tmux launcher; the session-state and
picker ideas were sharpened by [craftzdog/tmux-claude-session-manager](https://github.com/craftzdog/tmux-claude-session-manager).
