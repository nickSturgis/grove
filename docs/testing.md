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

## The fast-forward on open

`GROVE_CLAUDE=/bin/true` is enough: the pane falls through to a shell and the
session stays up, so kill it by name before every open or the sync (rightly)
never runs. Move main with an empty commit in the main checkout:

```bash
git commit -q --allow-empty -m on-main   # in the main checkout
grove -l                                 # expect ⇣1 on wt1
```

Open it from inside the worktree, same rules as above, and expect
`grove: fast-forwarded wt1 to main (+1)` with the marker gone from `grove -l`.
Then force each guard and confirm the marker still reads right but `grove/wt1`
stays put:

- an untracked file in the worktree — `~1 ⇣1`
- a commit on `grove/wt1`, then another on main — `⇡1⇣1`
- a live session — attaches, no sync
- the main checkout on a detached HEAD — no marker at all

`grove -s` from a second scratch repo must still show the first repo's marker:
that is what proves `base_ref` resolves per path.

## Adoption

Make the foreign worktree by hand — `git worktree add -b worktree-<name>
.claude/worktrees/<name>` is exactly what `claude -w` leaves:

```bash
grove -l                      # expect: <name>  unadopted  worktree-<name>
grove adopt <name>            # then free / grove/<name>
```

The refusals are the part worth forcing, because one of them renamed `main`
during development. `grove adopt` must reject `.`, `.claude`, `$wt_base`, a path
in another repo, a detached-HEAD worktree, and a name whose `grove/<slug>` branch
already exists — **check `git branch --list` is untouched after each.** A path
*inside* a worktree adopts that worktree, and re-adopting is a silent no-op.

Opening adopts too, from the picker and from a bare `grove` run inside the
worktree. Both go through `adopt_worktree`, so what those add is the launch after
it: expect the rename line, then a fast-forward if the tree is clean and behind,
then a `grove-<repo>-<name>` session. The busy guard needs a *second* tmux session
parked in that directory (`new-session -d -c <path>`) — the session grove itself
runs in is skipped, or running grove from inside the worktree could never work.

Prompts read from `/dev/tty`, so testing the confirm paths needs a pty:
`printf 'y\n' | script -qec 'grove -k wt1' /dev/null`. The `-f` path is the
opposite case — a dirty worktree plus `grove -k wt1 -f < /dev/null` must still
succeed, since answering up front is what makes `-k` work with no tty at all.

The lock states `-k` has to tell apart can all be forced with `git worktree
lock --reason`, no real claude needed:

```bash
st() { awk '{ sub(/^[0-9]+ \(.*\) /, ""); print $20 }' "/proc/$1/stat"; }

# stale — grove -k clears it and removes, no -f needed
git worktree lock --reason 'claude session wt1 (pid 999999 start 1)' <path>
# live — refused, and still refused under -f; the worktree and session survive
git worktree lock --reason "claude session wt1 (pid $$ start $(st $$))" <path>
# recycled pid — alive pid, wrong start time: must read as stale
git worktree lock --reason "claude session wt1 (pid $$ start 1)" <path>
# no reason at all — unattributable, so it reads as live and is refused
git worktree lock <path>
```

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
