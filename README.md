# grove

Launch Claude Code in an isolated git worktree, inside a tmux session that
outlives your terminal.

One bash file — no build step, no runtime, nothing to install but a symlink.

A grove is a stand of trees — here, the set of worktrees you have Claude
working in. Plant one per task, survey them, cut them down when merged.

```
grove             pick a worktree, or the main checkout; inside a worktree, open it
grove -n          always make a fresh worktree
grove -l          list this repo's worktrees and their state
grove -k <slug>   kill the session and remove the worktree
grove -k <s> -f   …and skip the are-you-sure when it still holds work
grove adopt <x>   take over a worktree another tool left behind
grove --hooks     print the Claude Code hook config for live status
grove -- <args>   pass args through to claude (e.g. grove -- --continue)
```

Each worktree gets its own branch (`grove/<slug>`) and its own tmux session
(`grove-<repo>-<slug>`), so parallel Claude sessions never touch each other's
files. Worktrees live at `<repo>/.claude/worktrees/<slug>` — the same place
Claude Code puts the ones `claude -w` makes — so `/resume` from the main
checkout finds their sessions under `Ctrl+W`. They're added to
`.git/info/exclude`, so they never show up in `git status`.

grove only ever treats a worktree as its own if it is on a `grove/<slug>`
branch, so the ones `claude -w` makes sit in the same directory untouched — but
they are listed, as `unadopted`, and opening one adopts it: its branch is
renamed to `grove/<name>` and from then on it is an ordinary grove worktree.
`grove adopt <name>` does the rename without opening it. That's a one-way door
in one respect — Claude Code no longer finds that worktree by name, so its own
cleanup leaves it to `grove -k` — but it's just a branch rename, and
`git branch -m` puts it back.

The picker has a key for both halves of that. `ctrl-x` (`x` without fzf) kills a
session and leaves its worktree standing. `ctrl-d` (`d`) takes the worktree away
as well — but only one of grove's own that is clean and whose commits have all
landed somewhere else. With nothing to lose it never asks; anything else it
refuses and says why, and `grove -k` is the way through, with its prompt.

`grove -k` refuses to silently discard work: it warns and shows the commits if
the branch holds anything not merged anywhere else, then asks. `-f` answers that
question up front — the warning still prints — which is also what makes `-k`
usable with no terminal to prompt at.

A worktree Claude Code has entered carries a git lock naming the session holding
it, and a session that dies without releasing it leaves that lock behind. `-k`
clears one whose process is gone, and refuses one still running — even under
`-f`, since another agent is working in those files. Unlock that one by hand
(`git worktree unlock <path>`) if you know the process is unrelated.

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

Needs `git` and `tmux` (>= 3.2, for `new-session -e`); uses `fzf` for the picker
(with a live pane preview) if it's installed, otherwise a numbered prompt.

## Config

| Env | Default | |
|---|---|---|
| `GROVE_ROOT` | unset | set it to put worktrees at `$GROVE_ROOT/<repo>/<slug>` instead |
| `GROVE_CLAUDE` | `claude` | the binary to launch |
| `GROVE_PICKER` | `auto` | `auto`, `fzf`, or `plain` |

Into each session it creates, grove exports `GROVE_SLUG` and `GROVE_MAIN_ROOT`
(the main checkout's path) — so what runs inside knows it is in a grove worktree
without inferring it from the branch name, which a detached HEAD would hide.
Attaching to a session that already exists changes nothing, so a session
predating this gets neither.

## Credit

Started as our own worktree-per-task tmux launcher; the session-state and
picker ideas were sharpened by [craftzdog/tmux-claude-session-manager](https://github.com/craftzdog/tmux-claude-session-manager).

Unrelated to [ZiiMs/Grove](https://github.com/ZiiMs/Grove), a Rust TUI for
running several agent CLIs at once, which had the name first — we arrived at it
separately, from the trees.
