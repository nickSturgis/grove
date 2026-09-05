#!/usr/bin/env bash
# cc — launch Claude Code in an isolated git worktree, inside a tmux session
#      that outlives your terminal.
#
#   cc                pick a worktree, or make one if the repo has none
#   cc -n             always make a fresh worktree
#   cc -l             list this repo's cc worktrees and their state
#   cc -k <slug>      kill the session and remove the worktree
#   cc --hooks        print the Claude Code hook config for live status
#   cc -- <args>      pass args through to claude (e.g. cc -- --continue)
#
# Worktrees live outside the repo, under $CC_WORKTREE_ROOT
# (default ~/.cc-worktrees/<repo>/<slug>), so they never pollute your checkout.

set -euo pipefail

CC_WORKTREE_ROOT="${CC_WORKTREE_ROOT:-$HOME/.cc-worktrees}"
CC_CLAUDE="${CC_CLAUDE:-claude}"
CC_PICKER="${CC_PICKER:-auto}"          # auto | fzf | plain

self=$(readlink -f "$0")
die() { printf 'cc: %s\n' "$*" >&2; exit 1; }
sanitize() { printf '%s' "${1//[^a-zA-Z0-9_-]/_}"; }

# --- hook entrypoint: stamp state onto the tmux session ----------------------
# Runs as a Claude Code hook, from inside the session. Must stay before any
# git work so it stays fast and never fails a hook.
if [ "${1:-}" = "--state" ]; then
  [ -n "${TMUX:-}" ] || exit 0
  tmux set-option -t "${TMUX_PANE:-}" -q @cc_state "${2:-idle}" 2>/dev/null || true
  tmux set-option -t "${TMUX_PANE:-}" -q @cc_state_at "$(date +%s)" 2>/dev/null || true
  exit 0
fi

if [ "${1:-}" = "--hooks" ]; then
  cat <<EOF
Merge into ~/.claude/settings.json:

{
  "hooks": {
    "UserPromptSubmit": [
      { "matcher": "", "hooks": [ { "type": "command", "command": "$self --state working" } ] }
    ],
    "Notification": [
      { "matcher": "permission_prompt", "hooks": [ { "type": "command", "command": "$self --state waiting" } ] }
    ],
    "PreToolUse": [
      { "matcher": "AskUserQuestion", "hooks": [ { "type": "command", "command": "$self --state waiting" } ] }
    ],
    "Stop": [
      { "matcher": "", "hooks": [ { "type": "command", "command": "$self --state idle" } ] }
    ]
  }
}

Claude Code reloads hooks dynamically; no restart needed.
EOF
  exit 0
fi

# --- locate the main checkout, even when called from inside a worktree -------
command -v git  >/dev/null || die "git not found"
command -v tmux >/dev/null || die "tmux not found"

git_common=$(git rev-parse --git-common-dir 2>/dev/null) || die "not inside a git repository"
git_common=$(cd "$git_common" && pwd -P)
main_root=$(dirname "$git_common")
repo=$(basename "$main_root")
repo_slug=$(sanitize "$repo")
wt_base="$CC_WORKTREE_ROOT/$repo_slug"

session_for() { printf 'cc-%s-%s' "$repo_slug" "$(sanitize "$1")"; }

# --- one tmux round-trip for every session's attach state and status ---------
declare -A S_ATTACHED=() S_STATE=()
while IFS='|' read -r n a st; do
  [ -n "$n" ] || continue
  S_ATTACHED["$n"]=$a
  S_STATE["$n"]=$st
done < <(tmux list-sessions -F '#{session_name}|#{session_attached}|#{@cc_state}' 2>/dev/null || true)

alive()    { [ -n "${S_ATTACHED[$1]+x}" ]; }
attached() { [ "${S_ATTACHED[$1]:-0}" != 0 ]; }

# rank: lower sorts first. Sessions needing you float to the top.
status_of() {  # session -> "rank|label"
  local s=$1
  if ! alive "$s"; then printf '3|free'; return; fi
  local extra=""; attached "$s" && extra=" (attached)"
  case "${S_STATE[$s]:-}" in
    waiting) printf '0|needs you%s'  "$extra" ;;
    idle)    printf '1|done%s'       "$extra" ;;
    working) printf '4|working%s'    "$extra" ;;
    *)       printf '2|live%s'       "$extra" ;;
  esac
}

cc_worktrees() {
  git -C "$main_root" worktree list --porcelain 2>/dev/null \
    | sed -n 's/^worktree //p' \
    | while IFS= read -r p; do
        case "$p" in "$wt_base"/*) [ -d "$p" ] && printf '%s\n' "$p" ;; esac
      done
}

describe() {
  local p="$1" br dirty
  br=$(git -C "$p" symbolic-ref --short HEAD 2>/dev/null || echo detached)
  dirty=$(git -C "$p" status --porcelain 2>/dev/null | wc -l)
  [ "$dirty" -gt 0 ] && printf '%s ~%s' "$br" "$dirty" || printf '%s' "$br"
}

# --- machine-readable rows: display <TAB> session <TAB> path ------------------
rows() {
  local p slug session rank label
  while IFS= read -r p; do
    [ -n "$p" ] || continue
    slug=$(basename "$p"); session=$(session_for "$slug")
    IFS='|' read -r rank label <<<"$(status_of "$session")"
    printf '%s\t%-10s  %-20s  %s\t%s\t%s\n' \
      "$rank" "$slug" "$label" "$(describe "$p")" "$session" "$p"
  done < <(cc_worktrees) | sort -t$'\t' -k1,1n | cut -f2-
}
[ "${1:-}" = "--_rows" ] && { rows; exit 0; }

# --- create a fresh worktree, race-safe against parallel shells --------------
new_worktree() {
  mkdir -p "$wt_base"
  local n=1 slug path
  exec 9>"$wt_base/.lock"
  flock 9 2>/dev/null || true
  while :; do
    slug="wt$n"; path="$wt_base/$slug"
    [ ! -e "$path" ] && ! git -C "$main_root" show-ref --verify --quiet "refs/heads/cc/$slug" && break
    n=$((n + 1))
  done
  git -C "$main_root" worktree add -b "cc/$slug" "$path" >&2
  exec 9>&-
  printf '%s\n' "$path"
}

launch() {
  local path="$1"; shift
  local slug session cmd
  slug=$(basename "$path"); session=$(session_for "$slug")
  if ! alive "$session"; then
    cmd=$(printf '%q' "$CC_CLAUDE")
    for a in "$@"; do cmd="$cmd $(printf '%q' "$a")"; done
    cmd="$cmd; exec ${SHELL:-/bin/bash}"          # survive /exit
    tmux new-session -d -s "$session" -c "$path" "$cmd"
  fi
  [ -n "${TMUX:-}" ] && tmux switch-client -t "=$session" || tmux attach-session -t "=$session"
}

cmd_kill() {
  local slug="${1:-}" path session
  [ -n "$slug" ] || die "usage: cc -k <slug>"
  path="$wt_base/$slug"; session=$(session_for "$slug")
  [ -d "$path" ] || die "no such worktree: $slug"
  if [ -n "$(git -C "$path" status --porcelain 2>/dev/null)" ]; then
    printf 'cc: %s has uncommitted changes. Remove anyway? [y/N] ' "$slug" >&2
    read -r a </dev/tty; [ "$a" = y ] || exit 1
  fi
  alive "$session" && tmux kill-session -t "=$session"
  git -C "$main_root" worktree remove --force "$path"
  git -C "$main_root" branch -D "cc/$slug" 2>/dev/null || true
  printf 'cc: removed %s\n' "$slug" >&2
}

force_new=0
case "${1:-}" in
  -l|--list) rows | cut -f1 | sed 's/^/  /'; exit 0 ;;
  -k|--kill) shift; cmd_kill "${1:-}"; exit 0 ;;
  -n|--new)  force_new=1; shift ;;
  -h|--help) sed -n '2,13p' "$self" | sed 's/^# \{0,1\}//'; exit 0 ;;
  --)        shift ;;
esac

# already inside a cc worktree? go straight to its session
here=$(pwd -P)
if [ "$force_new" = 0 ] && [ "$here" != "${here#"$wt_base"/}" ]; then
  launch "$wt_base/$(printf '%s' "${here#"$wt_base"/}" | cut -d/ -f1)" "$@"
  exit 0
fi

[ "$force_new" = 1 ] && { launch "$(new_worktree)" "$@"; exit 0; }

mapfile -t ROWS < <(rows)
[ "${#ROWS[@]}" -eq 0 ] && { launch "$(new_worktree)" "$@"; exit 0; }

# --- picker ------------------------------------------------------------------
use_fzf=0
case "$CC_PICKER" in
  fzf)  use_fzf=1 ;;
  auto) command -v fzf >/dev/null && [ -t 1 ] && use_fzf=1 ;;
esac

if [ "$use_fzf" = 1 ]; then
  out=$(printf '%s\n' "${ROWS[@]}" | fzf \
    --delimiter='\t' --with-nth=1 --height=70% --reverse --no-multi \
    --header=$'enter open   ctrl-x kill   ctrl-n new worktree   esc quit\n' \
    --preview='tmux capture-pane -pe -t "={2}:" 2>/dev/null | tail -40 || echo "(not running)"' \
    --preview-window='right,58%,border-left' \
    --bind="ctrl-x:execute-silent(tmux kill-session -t '={2}' 2>/dev/null)+reload($self --_rows)" \
    --expect=ctrl-n) || true
  key=$(printf '%s' "$out" | head -1)
  sel=$(printf '%s' "$out" | sed -n 2p)
  [ "$key" = ctrl-n ] && { launch "$(new_worktree)" "$@"; exit 0; }
  [ -z "$sel" ] && exit 0
  launch "$(printf '%s' "$sel" | cut -f3)" "$@"
  exit 0
fi

printf '\n  %s — pick a worktree\n\n' "$repo" >&2
for i in "${!ROWS[@]}"; do
  printf '  %d) %s\n' "$((i + 1))" "$(printf '%s' "${ROWS[$i]}" | cut -f1)" >&2
done
printf '  n) new worktree\n  q) quit\n\n  > ' >&2
read -r choice </dev/tty
case "$choice" in
  n|N)    launch "$(new_worktree)" "$@" ;;
  q|Q|'') exit 0 ;;
  *)
    [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le "${#ROWS[@]}" ] \
      || die "invalid choice: $choice"
    launch "$(printf '%s' "${ROWS[$((choice - 1))]}" | cut -f3)" "$@"
    ;;
esac
