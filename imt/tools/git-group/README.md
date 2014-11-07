git-group
=========
*git-group* is an extention to Git utility which allows to operate with Git submodules, installed in a Git superproject, in a synchronous manner.

# Usage:
```
$ git group <subcommand>
```

# Subcommands:
* **status** -- Show status
```
$ git group status
```

* **branch** -- Show active branches
```
$ git group branch
```

* **switch-branch** -- Switch to new branch (on all repositories in the group)
```
$ git group switch-branch <branch-name>
```

* **update** -- Checkout local repositories from the remotes to the latest code. For submodules -- update to the current commits.
```
$ git group pull
```

* **pull** -- Checkout local repositories from the remotes to the latest code, including submodule.
```
$ git group pull
```

