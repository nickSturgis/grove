# Git workflow

The only description of it — don't restate it in a skill, an `AGENTS.md`, a
plan, or a subdirectory. Link here instead.

Work happens in a worktree branched from **local HEAD**, not `origin`, so
unmerged commits are present (`worktree.baseRef: "head"` in
`.claude/settings.json`). Branching fresh silently bases the work on stale
files, and the mistake only shows up as a bad merge — or as a superseded rule
in an `AGENTS.md` that reads exactly like a current one.

1. `EnterWorktree` before the first edit. Nothing enforces this — an edit made
   in the main checkout can land on top of another session's in-flight work.
2. Do the work. Commit after each meaningful update.
3. **Stop.** Landing is the owner's call — wait for "land on main" or similar.
4. Then, in order: `ExitWorktree` **keeping** the worktree · `git merge --no-ff`
   onto `main` · **verify the work is really on `main`** (`git log`, and the
   changed files) · only then remove the worktree and delete the branch
   (`git branch -d`, which refuses if unmerged).
5. **`git push`.** Merging to local `main` is only half of landing — push so
   `origin/main` matches. Skipping it leaves the work in exactly one place,
   and because the next worktree branches from local HEAD rather than
   `origin`, nothing downstream will notice it never left this machine.

There are no Pull Requests in this repo, and no PR queue to triage. The trunk
is `main`, and every branch reaches it through step 4.

## Concurrent sessions

Several Claude sessions may be in this repo at once. Unexpected git state you
didn't create — a dirty tree, an in-progress merge, an unfamiliar branch or
worktree — belongs to another session. Don't investigate it, don't resolve it,
don't report it as a finding.

The stash stack is shared across worktrees. Never use bare `git stash` /
`git stash pop`; prefer a temporary WIP commit to set work aside.
