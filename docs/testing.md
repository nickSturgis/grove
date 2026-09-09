# Testing grove

No test suite. grove is verified by hand; this is the procedure.

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
