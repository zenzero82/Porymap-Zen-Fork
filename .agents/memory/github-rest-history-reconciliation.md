---
name: GitHub REST history reconciliation
description: How to make a local Git repository usable after its GitHub remote was populated through API commits.
---

When a repository is created or populated through GitHub’s REST API rather than a native `git push`, its commit history is separate from the local repository even when the file trees look the same. Fetch the remote, preserve a backup branch, and reconcile the histories before treating the remote as a normal Git upstream. If the remote content is authoritative, reset the tracked branch to the fetched remote after preserving the local branch; otherwise merge histories deliberately.

**Why:** A matching repository URL alone does not give a local branch an `origin/main` ancestor or upstream tracking. Future pulls and pushes can otherwise diverge or require an unsafe forced update.

**How to apply:** Verify that the tracked local branch and `origin/main` have the intended ancestry and, when the working copy should mirror GitHub, identical commit IDs. Test write access with a dry-run push using a protected runtime credential; never embed that credential in the repository URL or a committed file.