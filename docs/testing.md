# Testing grove

`tests/run` is the scriptable half: argv in, text and git state out. It builds
its own scratch repos and its own tmux server, needs no setup and leaves nothing
behind. Run it first, and extend it with the code — a new refusal, row marker or
entrypoint belongs in it.

```bash
tests/run                # every group
tests/run -l             # list them
tests/run rows kill -v   # two of them, echoing each invocation
```

What no script can reach is below: the two pickers, `attach`, and the
stale-claude restart. All of it drives tmux, so read the isolation rules first —
a teardown on the owner's server has already cost days. `GROVE_CLAUDE=/bin/true`
is enough for any of it: the pane falls through to a shell, so the session stays
up.

## Isolating the run

Give it its own tmux server, and `env -u TMUX` every command:

```bash
export TMUX_TMPDIR=/tmp/grove-test-tmux && mkdir -p "$TMUX_TMPDIR"
env -u TMUX grove -n      # every tmux call grove makes now lands on the test server
```

**`TMUX_TMPDIR` alone isolates nothing**, and the way it fails is silent. A tmux
client with `$TMUX` set connects to the socket named in that variable and ignores
`TMUX_TMPDIR` entirely — so inside a tmux pane (the normal case: agents run
inside grove sessions) the "test" server *is* the owner's, and a `kill-server`
you believe is scoped to your run destroys theirs. `env -u TMUX` is what makes
the isolation real; it is also what stops grove's `switch-client` from yanking
your own terminal into the test session. Prove it before trusting it:

```bash
env -u TMUX TMUX_TMPDIR=/tmp/grove-test-tmux tmux list-sessions   # test sessions only
ls "$TMUX_TMPDIR/tmux-$(id -u)/"                                  # its own socket exists
```

Only with that proven is `kill-server` ever the right verb — and detach every
client first, or the shutdown cannot finish (see the last section).

**Whenever the run is not provably on its own socket, tear down by name, never by
sweep**, and do not call `kill-server` at all:

```bash
tmux list-sessions -F '#{session_name}' | grep -E '^(grove-sr|driver|pick)' |
  while read -r s; do tmux kill-session -t "=$s"; done
```

A bare `for s in $(tmux list-sessions -F '#{session_name}')` has already killed a
live working session — and took the last session, so the server went with it and
the owner's next `grove` raced a shutting-down server.

## The pickers

Both are cursor UIs now, so driving them takes a real terminal: run grove in a
detached tmux session, `send-keys -t =<session>:` the arrows, then `capture-pane`
to read the frame. Check `GROVE_PICKER=fzf` and `GROVE_PICKER=plain` separately —
they are separate code paths and only one gets exercised on any given machine.

- ←/→ tabs on both. The sessions tab needs a **second** scratch repo, or it never
  proves it resolves the right session for a row outside the current one.
  `grove -s` from `~` is the no-repo path.
- `ctrl-x`/`x` on a live row, `ctrl-d`/`d` over `safe_remove`'s cases. The
  outcome rides fzf's header line via `$GROVE_NOTE` and the plain picker's own
  frame. Watch the frame, not just the text: the note adds a line, so switching
  tabs afterwards has to leave no stranded rows.
- `enter` on `+ new`, on a live session, on an unadopted row, on a `gone` row.
- `enter` on the **main checkout's row** — the last one before `+ new`. No
  script can reach this path, and it is the only one that starts a session
  outside a worktree. It must come up as `grove-<repo>-<repo>` in the repo root
  with `GROVE_MAIN_ROOT` set and **no** `GROVE_SLUG`, which is what stops an
  agent reading the repo itself as a worktree `-k` may throw away:

  ```bash
  tmux show-environment -t =grove-<repo>-<repo> GROVE_SLUG   # expect: -GROVE_SLUG
  ```

  On that same row, `ctrl-x`/`x` ends the session like any other, and
  `ctrl-d`/`d` must refuse with `is the main checkout`.
- Redirect stderr, or `TERM=dumb`, for the numbered fallback.

Force the rows worth looking at by hand: `tmux set-option -t <session>
@grove_state waiting`, and `git worktree remove --force` a live session's
directory to get the `gone` row.

## Attach

`tests/run` stops short of `attach` — it has no terminal, so grove's last step
fails there by design. From a real terminal, check both halves once: from outside
tmux grove attaches, from inside it switches the client, and `/exit` in the pane
leaves a shell rather than taking the session down.

## The stale-claude restart

Fake the installer's own layout — a symlink at a versioned binary, so flipping
the symlink *is* an update. Have the fake claude log its argv and then sleep, or
the pane falls straight through to the shell:

```bash
mkdir -p bin/v1 bin/v2
for v in 1 2; do printf '#!/bin/sh\necho "ARGV: $*" >>argv.log\nsleep 600\n' >bin/v$v/2.1.$v
  chmod +x bin/v$v/2.1.$v; done
ln -sfn "$PWD/bin/v1/2.1.1" bin/claude; export GROVE_CLAUDE="$PWD/bin/claude"
grove -n                                 # stamps @grove_claude
ln -sfn "$PWD/bin/v2/2.1.2" bin/claude   # "claude updated"
grove -l                                 # expect ↑2.1.2
```

Then force each guard in turn and watch `argv.log`: only the last two restart,
and only the idle one carries `--continue`.

- `@grove_state working` (and `waiting`) — marker, no restart
- attached: `tmux new-session -d -s driver "TMUX= tmux attach -t =<session>"`
- `@grove_claude ''` — a pre-stamp session, never stale
- `@grove_state idle`, detached — restarts `--continue`, and `@grove_claude` moves on
- unset (`set-option -u @grove_state`), detached — restarts with no `--continue`:
  a session no prompt has ever run in reads as idle and has nothing to resume

**Run it from inside the worktree**, not the repo root: from the root you get the
picker, and the restart never runs.

## A wedged server reads like a crash

A tmux server whose clients' terminals have died can never finish the shutdown a
teardown started. It **stays alive and listening** — it `accept()`s every new
connection and closes it instantly with no reply, so every tmux command in every
project fails with `server exited unexpectedly`. That message reads like a crash
and is the opposite: `ps` shows the server running, and `ss -xlp` shows it still
holding its socket. The blast radius reaches past tmux: claude processes orphaned
to PID 1 get SIGSTOPped (`SIGTTIN`, for the orphaned process group) and can no
longer reap their MCP children.

Clear it by killing the **clients**, not the server. Once the last stuck client
goes, the server completes its shutdown and exits on its own, so `kill -9` on the
server is never the fix:

```bash
ps -eo pid,stat,tty,args | grep '[t]mux attach-session'   # a stuck client's tty is gone
kill <pid>...                                             # server then exits by itself
```

Match on the argv, not the process name: tmux renames itself to `tmux: server`
and `tmux: client`, so `ps -C tmux` finds nothing at all — including the server
you are trying to prove is still alive.
