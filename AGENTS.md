# Project Guidelines

## General Principles

- Understand the existing architecture before making changes.
- Prefer small, focused changes over broad refactoring.
- Do not introduce abstractions unless they solve an actual current problem.
- Do not change unrelated code while implementing a feature or fixing a bug.
- Preserve existing behavior unless a behavior change is explicitly required.
- Before making non-trivial changes, explain the intended approach and its impact.
- Prefer extending the existing architecture over introducing parallel mechanisms.
- Avoid speculative functionality and premature optimization.

## Code Changes

- Keep domain logic independent from infrastructure and transport details.
- Respect the existing dependency direction between layers.
- Keep device/protocol-specific logic out of the domain layer.
- Reuse existing utilities and services before introducing new ones.
- Do not perform mass refactoring as part of a functional change.
- Formatting-only changes should be kept separate from functional changes whenever practical.
- Do not modify generated, temporary, or build files unless explicitly required.

## Error Handling

- Do not silently ignore errors.
- Preserve the existing error-handling strategy of the component being modified.
- When changing error handling, explain why the new behavior is preferable.
- Avoid catching exceptions unless there is a clear boundary where they should be converted or handled.

## Build and Tests

- Use the existing CMake build configuration.
- Prefer building through the existing build directory rather than creating additional build systems or configurations.
- Run the relevant tests after functional changes.
- For changes affecting multiple components or public interfaces, run the full test suite.
- For code changes, do not consider the change complete until the project builds successfully and the relevant tests pass.
- For changes affecting runtime behavior, perform a manual runtime check when practical.
- Do not modify tests merely to make them pass. If a test is incorrect, explain why it needs to change.
- Keep test changes focused on the behavior being changed.

## Git Workflow

- Use feature branches for new functionality, fixes, refactoring, and documentation changes.
- Keep commits focused on one logical change.
- Prefer a small number of meaningful commits over many trivial commits.
- Do not rewrite published history unless explicitly requested.
- Do not commit build artifacts, generated files, local configuration, or secrets.
- Before committing:
  - review the diff;
  - run `git diff --check`;
  - build the affected targets;
  - run the relevant tests.
- Before pushing, make sure the working tree contains only intentional changes.
- Do not mix unrelated functional changes with refactoring.
- Large formatting-only changes should be performed separately from functional changes.
- Commit messages should describe the logical purpose of the change.

## Documentation

- Keep documentation consistent with the current implementation.
- Update documentation when an architectural decision, public API, protocol, or development workflow changes.
- Prefer documenting important decisions and constraints rather than obvious implementation details.
- Keep API and protocol contracts explicit, including message formats and endpoint behavior.
- Do not duplicate the same information across multiple documents without a clear reason.
- When documentation becomes outdated, update or remove it rather than preserving obsolete information.
