# Gotchas found the hard way

Each of these cost an afternoon. They are the reason a line of `grove` looks the
way it does; `AGENTS.md` routes here rather than carrying them.

- fzf substitutes `{2}` as a *shell-quoted* string, so it must never sit inside
  quotes: `-t ={2}:` works, `-t "={2}:"` passes literal quote characters.
- `git rev-list --exclude=<glob>` globs are relative to the ref-listing option
  that follows, so it's `--exclude=grove/wt1 --branches`, not
  `--exclude=refs/heads/grove/wt1`. And `--all` silently includes every
  worktree's HEAD, which would always protect the branch you're testing.
- `a | b || c` binds `||` to `b`. Wrap the fallback: `{ a || c; } | b`.
- `printf '%-10s'` counts ANSI escape bytes as width, so pad the plain string and
  wrap the padded result in colour — never colour first.
- tmux `send-keys`/`capture-pane`/`respawn-pane`/`set-option` take a *pane* target,
  so `-t =<session>` fails with "can't find pane" where `kill-session` accepts it.
  The trailing colon is required — and on `set-option -q` the failure is silent,
  so the option simply never gets set.
- The plain picker marks the current row with a pointer, not reverse video: each row
  carries its own colour resets, which would cancel a reverse attribute mid-line.
- `exit-empty` (on by default) stops the tmux **server** the moment its last
  session dies, and a server mid-shutdown still accepts `new-session` before
  taking that session down with it. `launch` therefore `has-session`-checks
  before attaching: without it the client prints a bare `[exited]`, which reads
  like grove crashed when nothing of grove's went wrong.
- fzf `change-header` needs 0.42; Debian still ships **0.38**, which dies with
  "unknown action" on an unknown binding — taking the whole picker with it. That
  is why the tab bar rides in on `--header-lines`. Check any new binding against
  0.38 before using it.
- The plain picker's frame changes height when you switch tabs, so it rewinds by
  the count it actually drew (`FRAME`) and blanks the surplus lines — rewinding
  by row count alone leaves the old tab's rows stranded below.
- fzf binds bare letters at the cost of the query line, so a picker key that has
  to coexist with typing needs a modifier: the remove key is `ctrl-d` there and
  plain `d` only in the cursor picker, which has no query. `ctrl-d` costs
  half-page-down, which a one-row-per-worktree list never needed.
