# Making tmux feel like a normal terminal (mouse scroll)

Research note. Measured on this machine, 2026-09-06: tmux **3.3a**, Claude Code
**2.1.263**, VS Code integrated terminal, `TERM=xterm-256color`.

## The answer

```tmux
# ~/.tmux.conf
set -g mouse on
set -g history-limit 100000
```

That's it. The wheel now scrolls Claude Code's chat history, and copy mode
disappears on its own when you reach the bottom.

Reload with `tmux source-file ~/.tmux.conf`. **`history-limit` only applies to
windows created after it is set** — existing sessions keep whatever limit they
were born with and must be recreated.

## Why it's only two lines

Nearly every tmux-scrolling guide on the internet is written for the case where
the app inside the pane owns the alternate screen and the mouse. Measure before
believing any of it:

```console
$ tmux list-panes -a -F '#{session_name} cmd=#{pane_current_command} mouse_any=#{mouse_any_flag} alt=#{alternate_on} hist=#{history_size}/#{history_limit}'
0                     cmd=claude  mouse_any=0  alt=0  hist=1922/2000
4                     cmd=claude  mouse_any=0  alt=0  hist=763/2000
grove-kubernetes-wt1  cmd=bash    mouse_any=0  alt=0  hist=1898/2000
```

`alt=0` and `mouse_any=0` on a live `claude` pane are the whole story. Claude
Code has two renderers, and `~/.claude/settings.json` here says `"tui": "default"`:

| | classic (`"tui": "default"`) | fullscreen (`"tui": "fullscreen"`) |
|---|---|---|
| screen buffer | normal — output flows into tmux scrollback | alternate |
| requests mouse tracking | no (`mouse_any_flag=0`) | yes (`mouse_any_flag=1`) |
| wheel, with `mouse on` | tmux `copy-mode -e` over **real scrollback** | tmux `send -M` → Claude scrolls its own viewport |
| tmux copy-mode / search | sees the whole conversation | sees nothing |

On the classic renderer the conversation is *already* in tmux's scrollback. You
are not fighting the alternate screen; you just have `mouse off`.

Note `hist=1922/2000`: the default 2000-line limit was already discarding old
chat history. `history-limit` is not a nice-to-have here, it's half the fix.

## `mouse on` routes events, it does not steal them

The common fear — "mouse on means the app stops getting mouse input" — is
backwards. From `server_client_reset_state()` in tmux's `server-client.c`:

- **`mouse off`** — tmux passes the pane's own mouse mode through to the outer
  terminal. If the app asked for tracking, it gets it; tmux never sees a wheel
  event.
- **`mouse on`** — tmux *overrides* the pane's request, forcing DECSET 1002/1003
  on the outer terminal so **tmux** receives every event. It then consults its key
  bindings and, for `send -M`, re-encodes the event in whatever encoding the pane
  negotiated (SGR 1006 / UTF-8 1005 / legacy X10) with **pane-relative**
  coordinates. That last part is why mouse-aware apps work correctly in split
  panes under tmux but not under a bare terminal.

`send -M` is a silent no-op when the app has not enabled tracking — `input_key_mouse()`
returns early on `(s->mode & ALL_MOUSE_MODES) == 0`. That is exactly why the
built-in binding tests `#{mouse_any_flag}` before forwarding.

## Do not add wheel bindings — tmux 3.3a already has them

```console
$ tmux list-keys -T root | grep WheelUpPane
bind-key -T root WheelUpPane if-shell -F "#{||:#{pane_in_mode},#{mouse_any_flag}}" { send-keys -M } { copy-mode -e }
```

This has shipped as a default since ~2.4. The widely-copied
`bind -n WheelUpPane if-shell -F -t = "#{mouse_any_flag}" ...` recipe is that
same logic, hand-written. Pasting it in buys nothing and risks getting it wrong.

The dispatch on a wheel-up:

```
app requested mouse tracking?
  yes -> send -M            forward the event to the app
  no  -> already in copy mode?
           yes -> send -M   -> copy-mode table -> scroll-up 5 lines
           no  -> copy-mode -e
```

`copy-mode -e` is what makes it feel native: `-e` exits copy mode when you scroll
back to the bottom. Caveat from `man tmux` — *"while in copy mode, pressing a key
other than those used for scrolling will disable this behaviour"*, so if you start
a selection you'll need `q` to get out.

There is deliberately **no** `WheelDownPane` in the `root` table. Outside copy
mode there is nothing below the bottom to scroll to.

## Myths, with the command that disproves each

**`alternate-scroll` is not a tmux thing.** Not an option, not a
`terminal-features` value, not a terminfo extension, at any version. It is an
xterm resource (`alternateScroll`) and an iTerm2 preference, applied by the outer
terminal — and irrelevant once tmux is consuming and re-emitting wheel events.

```console
$ man tmux | grep -c alternate-scroll
0
```

So `set -as terminal-features ',*:alternate-scroll'` is a no-op. Delete it.

**Don't disable the alternate screen.** Both of these are cargo-cult from the
pre-2015 era:

```tmux
set -g alternate-screen off                          # no
set -ga terminal-overrides ',xterm*:smcup@:rmcup@'   # no (the older hack for the same thing)
```

They stop tmux switching to the alternate grid so full-screen apps scroll into
the main history. For a *continuously redrawing* TUI that means every keystroke
dumps a fresh screenful into scrollback — your history becomes an unreadable
flipbook of partial frames — and `less`/`vim` no longer restore the screen on
exit. Reported not to work reliably in claude-code#67289. Irrelevant here anyway,
since the classic renderer never touches the alternate screen.

**iTerm2's "scroll wheel sends arrow keys in alternate screen mode"** (Settings →
Advanced → Mouse) is a misfeature under tmux: the wheel emits `Up`/`Down`, which
walks shell history in a shell and input-box history in Claude Code. That's the
symptom in claude-code#58364. Not applicable to the VS Code terminal, but worth
knowing if you move terminals.

## Optional extras

Each with its cost. None are required for scrolling.

```tmux
set -sg escape-time 10        # default 500ms makes Esc feel broken in TUIs
set -g focus-events on        # forward focus in/out to apps that use them
set -g mode-keys vi           # copy-mode gets /search, n/N, g/G, C-u/C-d, v/y

set -g set-clipboard on       # OSC 52 — copy reaches the system clipboard over SSH
set -as terminal-features ',xterm-256color:clipboard'
set -as terminal-features ',xterm-256color:RGB'        # truecolor
```

- **`escape-time 10`, not `0`.** The folklore value is 0, but it can misparse real
  escape sequences on a laggy SSH/mosh link. 10 is what Neovim's `:checkhealth`
  recommends.
- **`terminal-features` patterns match the OUTER `$TERM`** — what tmux saw when the
  server started (`xterm-256color` here), *not* `default-terminal`
  (`tmux-256color`). And always `set -as` to append; a bare `set -g` clobbers
  tmux's built-in detection table.
- **`set-clipboard on`** needs an `Ms` terminfo entry and a terminal that permits
  OSC 52. iTerm2 blocks it until Settings → General → Selection → *Applications in
  terminal may access clipboard*. Use `external` instead of `on` to let tmux write
  your terminal's clipboard but ignore apps writing tmux buffers.
- **Wheel speed** is 5 lines/notch by default; most terminals do 3. To match:
  ```tmux
  bind -T copy-mode    WheelUpPane select-pane \; send -N3 -X scroll-up
  bind -T copy-mode-vi WheelUpPane select-pane \; send -N3 -X scroll-up
  ```
  (and the `scroll-down` equivalents). Overriding the *copy-mode* table is safe;
  overriding the *root* table is what you must not do.
- **`aggressive-resize`** — `man tmux` is explicit: *"good for full-screen programs
  which support SIGWINCH and poor for interactive programs such as shells."* Grove
  sessions run Claude Code, but `grove` execs a shell after `/exit`, so leave it off.
- **`allow-passthrough on`** lets panes emit raw escapes to the real terminal
  (sixel/kitty images, OSC 8 hyperlinks). Small trust cost: a program in the pane
  can write arbitrary bytes to your terminal.
- **Memory**: roughly `history-limit × panes × per-line size`. 100k lines across a
  dozen panes is tens to low hundreds of MB. Drop to 50000 if you keep many panes.

## Text selection after `mouse on`

With `mouse on`, tmux enables mouse reporting on the real terminal, so the
emulator stops doing native click-and-drag selection. **Hold Shift** to suppress
reporting and get it back — the xterm convention, honoured by VS Code's terminal,
kitty, Alacritty, WezTerm, Ghostty, GNOME Terminal, Konsole, foot, Windows
Terminal. (iTerm2 uses Option; Terminal.app uses Fn.)

The catch: a Shift-drag selects across the whole terminal window and knows
nothing about panes, so a split layout yields interleaved garbage and it grabs
the status line too. For single-pane selection tmux's own copy-mode drag is
better — and with `set-clipboard on` it reaches the system clipboard. If you want
a selection to survive without yanking you back to the bottom, replace the default
`copy-pipe-and-cancel`:

```tmux
bind -T copy-mode-vi MouseDragEnd1Pane send -X copy-pipe-no-clear
```

## If you ever switch to `/tui fullscreen`

Everything above inverts. Claude Code takes the alternate screen and requests
mouse tracking, so `mouse on` becomes **mandatory** (tmux forwards the wheel via
`send -M` and Claude scrolls its own viewport), and tmux copy-mode goes blind —
the alternate grid has zero history lines.

- `Ctrl+o` then `[` dumps the full transcript into native scrollback, at which
  point tmux search sees it again. `v` opens it in `$EDITOR`.
- `CLAUDE_CODE_DISABLE_ALTERNATE_SCREEN=1` forces the classic renderer.
- `CLAUDE_CODE_NO_FLICKER=1` + `CLAUDE_CODE_DISABLE_MOUSE=1` keeps flicker-free
  rendering but gives the mouse back to tmux — you lose wheel scrolling inside
  Claude Code (PgUp/PgDn/Ctrl+End still work) and keep native selection.
- `CLAUDE_CODE_SCROLL_SPEED=3` (or `/scroll-speed`) if the wheel feels sluggish.
- On tmux ≤3.5a a wheel-up on an alternate-screen pane with no app tracking still
  fires `copy-mode -e` and drops you into an empty copy mode. Fixed in 3.6.

## grove needs no changes

`grove` creates sessions with a plain `tmux new-session -d` (line 174), so they
inherit the global config. A `~/.tmux.conf` fixes every future grove session with
no change to the script — which is also what AGENTS.md wants, since `grove` stays
one single-purpose file.

Existing sessions keep `history-limit 2000` until recreated. `grove -k <slug>`
then `grove` gets you a fresh one.

## Upgrade note

3.3a is from 2022; upstream is 3.8. Relevant `CHANGES` entries:

- **3.6** — "Don't enter copy mode on mouse wheel in alternate screen."
- **3.6** — synchronized output (DECSET 2026), the mechanism Claude Code's
  fullscreen renderer uses to avoid tearing.
- **3.6** — `terminal-features` can disable a feature with a `@` suffix; new `utf8`
  feature. `allow-passthrough` gains `all`.
- **3.7 → 3.8** — "The mouse option now defaults to on." Upstream agrees.

None of these are needed for the classic renderer.

## Sources

- `man tmux` (3.3a, local) and [man.openbsd.org/tmux](https://man.openbsd.org/tmux)
- tmux 3.3a sources: [`key-bindings.c`](https://raw.githubusercontent.com/tmux/tmux/3.3a/key-bindings.c),
  [`server-client.c`](https://raw.githubusercontent.com/tmux/tmux/3.3a/server-client.c),
  [`input-keys.c`](https://raw.githubusercontent.com/tmux/tmux/3.3a/input-keys.c),
  [`tmux.h`](https://raw.githubusercontent.com/tmux/tmux/3.3a/tmux.h)
- [tmux CHANGES](https://raw.githubusercontent.com/tmux/tmux/master/CHANGES)
- Claude Code docs: [fullscreen rendering](https://code.claude.com/docs/en/fullscreen),
  [environment variables](https://code.claude.com/docs/en/env-vars)
- anthropics/claude-code issues
  [#9902](https://github.com/anthropics/claude-code/issues/9902),
  [#15780](https://github.com/anthropics/claude-code/issues/15780),
  [#58364](https://github.com/anthropics/claude-code/issues/58364),
  [#60185](https://github.com/anthropics/claude-code/issues/60185),
  [#67289](https://github.com/anthropics/claude-code/issues/67289)
