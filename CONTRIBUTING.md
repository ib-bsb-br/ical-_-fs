# Contributing

## Commit messages

Minor changes can be just a single word. Otherwise, try to describe
in the commit header what was done and in the description why. If
the commit is related to an issue, [make sure to reference that
ticket.](https://man.sr.ht/git.sr.ht/#referencing-tickets-in-git-commit-messages)

## Memory model

- `memory_region` is for operation-scoped memory and gets freed
automatically when the operation ends. Use it as much as possible
to avoid memory leaks.
- Tree nodes are heap allocated and must be managed manually.

Avoid mixing `memory_region` pointers with global state.


