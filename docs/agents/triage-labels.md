# Triage Labels

The skills speak in terms of five canonical triage roles. This file maps those roles to the actual label strings used in this repo's issue tracker.

| Label in mattpocock/skills | Label in our tracker | Meaning                                  |
| -------------------------- | -------------------- | ---------------------------------------- |
| `needs-triage`             | `needs-triage`       | Maintainer needs to evaluate this issue  |
| `needs-info`               | `needs-info`         | Waiting on reporter for more information |
| `ready-for-agent`          | `ready-for-agent`    | Fully specified, ready for an AFK agent  |
| `ready-for-human`          | `ready-for-human`    | Requires human implementation            |
| `wontfix`                  | `wontfix`            | Will not be actioned                     |

When a skill mentions a role (e.g. "apply the AFK-ready triage label"), use the corresponding label string from this table.

## Gitea label IDs

Only `watch` is recorded here. Resolve any other label's ID as
`docs/agents/issue-tracker.md#labels-are-ids-not-names` says.

| Name              | ID |
| ----------------- | -- |
| `watch`           | 46 (org) |

## Outside triage: `watch`

`watch` is not a triage role, and triage never assigns it, but it **fills the
state slot**: an issue carries one of the roles above *or* `watch`, never both,
and a Watch is not untriaged. It is an org label, so its ID is the same in every
glowlab repo. The rules are the `watch` skill's, section *The label, and
landing*.
