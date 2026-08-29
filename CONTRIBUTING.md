# Contributing to WireSpaces

Thank you for contributing. This repository uses pull requests, mandatory
review, and CI for all changes to `main`.

## Branch workflow

1. Branch from the latest `main`.
2. Make focused changes on a feature branch.
3. Open a pull request early for larger work.
4. Keep pull requests small when possible.
5. Merge only after CI passes and review is complete.

### Branch naming

Use descriptive prefixes:

- `feature/<short-description>`
- `fix/<short-description>`
- `docs/<short-description>`
- `agent/<short-description>`

Examples:

- `feature/sim-vcan-adapter`
- `docs/update-architecture-register`
- `agent/simulator-shell-tests`

Do not push directly to `main`.

## Local development

### Code

The C core, embedded C++ libraries, and host simulator build as one project
from the repository root.

Build and test on Linux or WSL:

```sh
./scripts/test.sh
```

Or manually:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The first configure may fetch GoogleTest into the local build tree. Git and
network access are required for that step.

### Documentation

Architecture and protocol documents live under `docs/`.

When changing architecture or behavior, update the relevant documents and the
control surface in `docs/architecture_register.md`. Add revision narrative to
`docs/history.md` when appropriate.

## Pull request expectations

Every pull request should include:

- a clear summary of what changed and why
- a test plan with commands run or an explicit statement that no tests apply
- documentation updates when behavior, architecture, or public API changes
- scope boundaries when the change is intentionally partial

Use the pull request template when opening a PR.

## Review policy

All changes to `main` require:

- a pull request
- passing CI
- at least one approving review from a human maintainer
- resolved review conversations before merge

Squash merge is the preferred integration method.

### Agent-authored changes

Agents may create branches, commit code, and open pull requests.

Agents must:

- work only on feature branches
- include a test plan and affected-document list in the PR
- keep changes scoped to the requested task
- update `docs/architecture_register.md` and `docs/history.md` when architecture
  or behavior changes

Agents must not:

- push directly to `main`
- merge their own pull requests
- treat agent review as sufficient approval when the same account also authored
  the pull request

### Agent review

Agents may review pull requests and leave actionable comments on correctness,
scope, tests, and document consistency.

Agent review is advisory unless it comes from a separate trusted bot identity.
A human maintainer must still approve before merge.

## Commit messages

Write concise commit messages that explain the why, not just the what.

Good:

- `Add GoogleTest-based simulator shell tests`
- `Document simulator CI workflow in CONTRIBUTING`

Avoid vague messages such as `fix stuff` or `update files`.

## Code style

Follow the project C++ rules in `Design/Instructions/cpp_rules.md` for C++ work.

Embedded library code should remain suitable for MCUs: no exceptions, no RTTI,
and no heap use outside one-time initialization unless the target is explicitly
host-only test code.

## Repository protection

`main` should remain protected with:

- required pull request reviews
- required status check: `Build and test`
- no direct pushes
- no force pushes
- stale review dismissal on new commits

These settings are configured in GitHub repository settings or rulesets.
