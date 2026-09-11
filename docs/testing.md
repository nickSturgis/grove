# Testing grove

No test suite. grove is verified by hand in a scratch repo; this is the procedure.

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

## The stale-claude restart

Fake the installer's own layout — a symlink at a versioned binary — so flipping
the symlink *is* an update. Have the fake claude log its argv and then sleep, or
the pane falls straight through to the shell:

```bash
mkdir -p bin/v1 bin/v2
for v in 1 2; do printf '#!/bin/sh\necho "ARGV: $*" >>argv.log\nsleep 600\n' >bin/v$v/2.1.$v
  chmod +x bin/v$v/2.1.$v; done
ln -sfn "$PWD/bin/v1/2.1.1" bin/claude; export GROVE_CLAUDE="$PWD/bin/claude"
grove -n                              # stamps @grove_claude
ln -sfn "$PWD/bin/v2/2.1.2" bin/claude   # "claude updated"
grove -l                              # expect ↑2.1.2
```

Then force each guard in turn and confirm only the last one restarts — watch
`argv.log` grow by exactly one `--continue` line:

- `@grove_state working` (and `waiting`, and unset) — marker, no restart
- attached: `tmux new-session -d -s driver "TMUX= tmux attach -t =<session>"`
- `@grove_claude ''` — a pre-stamp session, never stale
- `@grove_state idle`, detached — restarts, and `@grove_claude` moves on

**Run it from inside the worktree**, not the repo root: from the root you get the
picker, and the restart never runs. And `env -u TMUX`, always — grove attaches
with `switch-client` when `$TMUX` is set, which yanks your own terminal into the
test session.

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
