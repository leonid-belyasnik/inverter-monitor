# Development Guide

## Development Environment

The project is developed primarily on Ubuntu 24.04.

Main tools:

* C++ compiler (GCC or Clang)
* CMake
* Git
* Docker
* VS Code
* clang-format

Ninja is available as an optional CMake build system. The project should use its existing build configuration rather than introducing additional build configurations without a concrete reason.

The local development environment uses Docker for infrastructure services. GitLab CI builds and tests the project.

## Project Build

Use the existing CMake build directory and configuration.

Build the project with:

```bash
cmake --build build
```

This assumes that the existing `build` directory has already been configured successfully.

Avoid creating additional build directories or changing generators unless required.

## Local Infrastructure

The backend uses:

* PostgreSQL for persistent measurement storage.
* Mosquitto for MQTT communication.

Infrastructure services are run locally through Docker.

The MQTT client uses these environment variables:

* `MQTT_HOST` — broker hostname; defaults to `localhost`.
* `MQTT_PORT` — broker port; defaults to `1883`.
* `MQTT_USERNAME` — optional broker username. If unset or empty, the backend connects without authentication.
* `MQTT_PASSWORD` — optional broker password; requires `MQTT_USERNAME`. If the password is set without a username, the backend refuses to start.

The credentials are sent over plain MQTT unless the broker connection is protected separately; the backend does not configure TLS. The username and password are never written to the logs.

Database connection settings are read from the standard libpq `PG*` environment variables. See [Configuration](configuration.md) for all variables and defaults, and [Deployment](deployment.md) for running the stack on a server.

## Tests

Run the test suite with:

```bash
ctest --test-dir build --output-on-failure
```

For code changes:

* Run relevant tests after functional changes.
* Run the full test suite for changes affecting multiple components or public interfaces.
* Perform a manual runtime check when practical for changes affecting runtime behavior.
* Do not modify tests merely to make them pass. Explain and justify changes to incorrect tests.

Documentation-only changes generally do not require a build or test run. They should still be checked for consistency with the implementation and reviewed for formatting or Markdown errors.

## Runtime Verification

When verifying the telemetry ingestion flow, use the existing simulator and local infrastructure where practical.

The intended runtime verification flow is:

```text
Telemetry Simulator
        |
        v
     MQTT Broker
        |
        v
  inverter-monitor
        |
        +------> PostgreSQL
        |
        +------> WebSocket Clients
        |
        +------> REST Clients
```

A successful check confirms that telemetry can be received, parsed, persisted, and delivered to subscribed WebSocket clients.

The main `inverter-monitor` runtime starts REST HTTP on port `8080` and WebSocket on port `8081`. The separate `api-server` development executable still does not run its HTTP server. A successful runtime check for `inverter-monitor` can include REST endpoint availability as well as telemetry delivery to WebSocket clients.

## Code Formatting

The project uses `.clang-format`, based on LLVM style with project-specific settings.

The configuration includes:

* Four-column indentation.
* Tabs for indentation.
* A column limit of 100.
* Braces on the same line for control statements.
* Braces on the next line for classes and functions.
* Namespace opening braces on the same line.

Format modified C++ files when appropriate.

Whole-project formatting must be performed separately from functional changes and documentation-only changes.

`clang-format` formats existing code but does not automatically add missing braces to control statements.

## Static Analysis

Additional static-analysis tools may be introduced when there is a concrete need and the workflow has been agreed upon.

Do not introduce new mandatory tools or checks without considering their effect on local development and CI.

## Continuous Integration

GitLab CI builds the project and runs tests.

For code changes, verify that the relevant build and tests pass locally where practical, and check the CI result before considering the change complete.

Documentation-only changes should be reviewed for accuracy and consistency; a code build is not normally necessary for such changes.

## Git Workflow

Use feature branches for new functionality, fixes, refactoring, and documentation changes.

Examples:

```text
feature/websocket
docs/project-guidelines
style/clang-format
```

Keep commits focused on one logical change. Documentation files that form one coherent documentation update may be included in a single commit.

Before committing:

1. Review the diff.
2. Run `git diff --check`.
3. Build the affected targets and run relevant tests for code changes.
4. Confirm that only intended files are staged.

Before pushing, check the working tree and ensure that all changes are intentional.

Do not mix large formatting-only changes with functional changes.

## Documentation

Keep documentation consistent with the current implementation.

Update documentation when architectural decisions, public APIs, protocols, or development workflows change.

Document important decisions and constraints without duplicating the same information across multiple files unnecessarily.

When implementation and documentation disagree, verify the actual code and update the documentation to describe the current behavior accurately.

## Dependencies

Prefer existing project dependencies and utilities when they satisfy the requirement.

Before introducing a new dependency, consider whether it is necessary, how it affects the architecture, and whether it adds maintenance or deployment complexity.

## Troubleshooting

When investigating a problem:

1. Reproduce it and capture the relevant error output.
2. Identify the affected component and follow the existing data flow.
3. Make the smallest change that addresses the root cause.
4. Rebuild and run relevant tests for code changes.
5. Perform a runtime check when practical.
6. Review the final diff before committing.
