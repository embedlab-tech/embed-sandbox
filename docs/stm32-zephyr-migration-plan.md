# STM32 Nucleo / Zephyr Implementation Plan

## Goal

Build a standalone Zephyr-based firmware project for the **NUCLEO-G070RB** in this repository.

The firmware generates configurable UART log traffic on multiple interfaces simultaneously to exercise a desktop log collection tool.

## Clean-start assumptions

This repository is now treated as a **clean Zephyr project start**.

- No legacy firmware stack is part of this plan.
- No comparison or migration work from a previous implementation is part of this plan.
- The repo currently keeps only project documentation and CI workflow scaffolding.
- The implementation should start from an empty firmware tree and add only what is needed.

## Highest-priority requirement

The first stage is a **repo-local, isolated, storage-minimized Zephyr bootstrap**.

Before any firmware code:

- keep all dependencies inside this repository
- avoid interfering with other Zephyr installations/workspaces on the host
- minimize disk usage as far as Zephyr tooling realistically allows
- prefer selecting less up front over downloading broadly and pruning later

## Scope

### In scope

- Standalone Zephyr application in this repository
- Target board: **STM32G070RB Nucleo**
- CLI on **USART2 / ST-LINK VCP only**
- Log generation on **USART1, USART3, USART4**
- Runtime UART reconfiguration
- Runtime generator reconfiguration
- Predefined scenarios
- Runtime statistics
- Repo-local SDK/tooling/bootstrap scripts
- CI workflow updates for the standalone Zephyr project

### Out of scope

- Supporting multiple firmware stacks in parallel
- Building inside Zephyr samples
- Depending on host-global Zephyr SDK, workspace, or Python environment
- Future extensions beyond designing the code so they can be added cleanly

## Non-negotiable constraints

- **Standalone repo topology.** Everything needed for this project lives under the current repository.
- **Local isolation.** No dependence on host-global west config, SDK registration, Python venv, or module tree.
- **Minimal storage bias.** Start with the smallest workable setup.
- **Shell isolation.** CLI exists only on USART2.
- **Generator isolation.** USART1/3/4 are generator ports only.
- **Runtime reconfiguration.** Baud and generator settings change without reboot.
- **Non-blocking control path.** Shell must stay responsive while generators run.

## Target hardware map

| Function | Peripheral | Pins | Notes |
|---|---|---:|---|
| Zephyr console + shell | USART2 | PA2 / PA3 | ST-LINK Virtual COM Port |
| Log generator #1 | USART1 | PC4 / PC5 | External FTDI #1 |
| Log generator #2 | USART3 | PD8 / PD9 | External FTDI #2 |
| Log generator #3 | USART4 | PA0 / PA1 | External FTDI #3 |

## Recommended repository topology

```text
.
├── .github/
│   └── workflows/
├── boards/
│   └── nucleo_g070rb.overlay
├── docs/
│   └── stm32-zephyr-migration-plan.md
├── scripts/
│   ├── bootstrap_zephyr.sh
│   ├── setup_env.sh
│   └── cleanup_zephyr_deps.sh      # optional fallback
├── src/
│   ├── main.c
│   ├── uart_manager.c
│   ├── uart_manager.h
│   ├── generator.c
│   ├── generator.h
│   ├── scenarios.c
│   ├── scenarios.h
│   ├── shell_commands.c
│   ├── shell_commands.h
│   ├── statistics.c
│   └── statistics.h
├── third_party/
│   ├── zephyr/
│   └── modules/
│       └── hal/
│           ├── cmsis/
│           └── stm32/
├── toolchains/
│   ├── python/
│   └── zephyr-sdk-<ver>/
├── CMakeLists.txt
├── prj.conf
└── west.yml                        # optional metadata/helper manifest only
```

## Workspace strategy

### Decision

Use this repository as the application root, but **do not rely on a root-level west-managed workspace topology**.

### Why

Zephyr documents that a workspace with the **topdir itself as a Git repository** is not an officially supported topology. Since this repository root is already a Git repo, the safest approach is:

- keep dependencies in repo-local directories
- build with explicit environment variables
- treat west as optional tooling, not as the owner of the repo root layout

### Build model

Use repo-local paths:

- `ZEPHYR_BASE=<repo>/third_party/zephyr`
- `ZEPHYR_MODULES=<repo>/third_party/modules/hal/cmsis;<repo>/third_party/modules/hal/stm32`
- `ZEPHYR_TOOLCHAIN_VARIANT=zephyr`
- `ZEPHYR_SDK_INSTALL_DIR=<repo>/toolchains/zephyr-sdk-<ver>`

## Lightweight Zephyr setup plan

## 1. Python tooling isolation

Create a repo-local Python virtual environment, for example:

```text
toolchains/python/.venv
```

Install only what is needed for:

- `west`
- Zephyr Python requirements
- flashing/debug helpers actually used for this board

Do not install Zephyr tooling globally.

## 2. SDK minimization

Use the **Zephyr SDK minimal bundle** first.

Then, during SDK setup, install **only the ARM toolchain** needed for STM32 builds.

### SDK policy

- install under `toolchains/`
- keep it repo-local
- do not make it the host-global default
- drive usage through `scripts/setup_env.sh`

### Cleanup fallback

If the selected install still leaves significant unused payload, add a post-install cleanup script that:

- audits disk usage under `toolchains/zephyr-sdk-*`
- removes only clearly unused optional content
- stays conservative; do not prune anything required by the STM32 build/flash flow

Default policy: **minimize at install time first, clean later only if needed**.

## 3. Dependency minimization

Using upstream Zephyr’s full `west update` is not the preferred baseline for this project.

Reason:

- upstream manifest groups are too coarse for “STM32 only” disk minimization
- broad updates pull many modules that are irrelevant here

### Recommended dependency baseline

Clone only the minimal initial set under `third_party/`:

- `zephyr`
- `cmsis`
- `hal_stm32`

### Conditional additions

Only add another dependency when a build or flashing/debug requirement proves it is needed.

`hal_st` may be needed depending on the board/driver path. Treat that as build-proven, not assumed.

## 4. Bootstrap flow

Provide a deterministic repo-local bootstrap script that:

1. creates the repo-local Python venv
2. installs Python tooling into that venv
3. downloads the repo-local Zephyr SDK
4. installs only ARM toolchain support
5. clones only the pinned minimal dependency set into `third_party/`
6. writes or updates a repo-local environment setup script
7. proves the environment with a trivial Zephyr build for `nucleo_g070rb`
8. reports disk usage

## 5. Optional west usage

west may still be used for convenience commands, but the build must remain possible from explicit repo-local paths alone.

If `west.yml` exists, treat it as:

- pinned revision metadata
- optional helper manifest
- not a requirement for root workspace ownership

## Implementation phases

## Phase 0 — Local Zephyr bootstrap and footprint proof

### Work

- Create repo-local directories for tooling and dependencies.
- Add bootstrap scripts:
  - `scripts/bootstrap_zephyr.sh`
  - `scripts/setup_env.sh`
  - optional `scripts/cleanup_zephyr_deps.sh`
- Create repo-local Python venv.
- Install west and required Python packages into that venv.
- Download Zephyr SDK minimal bundle.
- Install only ARM toolchain support.
- Clone minimal dependency set into `third_party/`.
- Build a trivial Zephyr app for `nucleo_g070rb` using only repo-local paths.
- Measure disk usage.
- Add cleanup only if measured footprint justifies it.

### Acceptance

- No host-global Zephyr installation is required.
- All toolchain and dependency paths are inside this repo.
- A fresh shell can enter the environment through one local setup script.
- A trivial Zephyr build succeeds for `nucleo_g070rb`.
- Footprint is measured and recorded.

## Phase 1 — Project skeleton

### Work

- Add:
  - `CMakeLists.txt`
  - `prj.conf`
  - `boards/nucleo_g070rb.overlay`
  - `src/main.c`
- Enable:
  - UART
  - serial drivers
  - console
  - shell
  - logging
  - timing helpers needed for periodic generation
- Verify build using the repo-local setup.

### Acceptance

- Clean Zephyr build for the target board.
- Console and shell enabled at config level.
- No dependency on host-global Zephyr state.

## Phase 2 — UART bring-up

### Work

- Configure devicetree overlay so:
  - `zephyr,console` and `zephyr,shell-uart` use USART2
  - USART1/3/4 are enabled for application use
- Add startup validation for each UART device.
- Fail fast on missing devices.
- Verify shell on USART2 and test TX on USART1/3/4.

### Acceptance

- Shell reachable only through ST-LINK VCP.
- Each generator UART transmits known test data independently.
- No shell chatter appears on USART1/3/4.

## Phase 3 — UART manager

### Responsibilities

- Resolve and retain generator UART device handles.
- Track per-UART runtime config:
  - logical ID
  - enabled state
  - baudrate
  - current mode
- Apply runtime baud changes with `uart_configure()` where supported.
- Provide a small explicit API.

### Suggested API

```c
int uart_manager_init(void);
const struct uart_port *uart_get(enum uart_id id);
int uart_set_baud(enum uart_id id, uint32_t baud);
int uart_enable(enum uart_id id);
int uart_disable(enum uart_id id);
int uart_send(enum uart_id id, const uint8_t *buf, size_t len);
```

### Acceptance

- Runtime baud changes work on generator UARTs without reboot.
- Disabled UARTs stop transmitting.
- Shell UART is unaffected by generator configuration.

## Phase 4 — Log generator core

### Responsibilities

- One generator instance per output UART.
- Per-generator runtime state:
  - enabled flag
  - interval
  - payload size
  - counter
  - template/format selection
  - random mode
- Periodic emission without blocking shell handling.

### Implementation direction

Default to **one dedicated generator thread per UART** unless a work-queue model proves simpler without hurting responsiveness.

### Output examples

```text
[UART1] INFO Boot completed
[UART1] INFO Counter=1523
[UART3] ERROR Sensor timeout
[UART4] DEBUG Packet 845
```

### Acceptance

- Three generators run concurrently.
- Per-UART settings are independent.
- Shell stays responsive while all generators are active.

## Phase 5 — Runtime configuration and shell commands

### Command groups

#### UART control

```text
uart list
uart <id> baud <value>
uart <id> enable
uart <id> disable
```

#### Generator control

```text
gen start <id>
gen stop <id>
gen interval <id> <ms>
gen size <id> <bytes>
gen random <id> on
gen random <id> off
gen template <id> <name>
```

#### Scenario control

```text
scenario list
scenario run <name>
scenario stop
```

#### Statistics

```text
stats
stats <id>
```

### Acceptance

- Full runtime reconfiguration through Zephyr shell.
- No reboot required for supported changes.
- Invalid input fails cleanly.

## Phase 6 — Scenarios

### Scenarios to implement

1. **continuous** — steady INFO traffic
2. **burst** — large rapid message burst
3. **random** — random sizes and intervals within bounded limits
4. **high-throughput** — highest sustainable transmit rate per configured UART
5. **mixed** — different baud/interval/size per UART
6. **baud-switching** — periodic baud changes across:
   - 9600
   - 19200
   - 38400
   - 57600
   - 115200
   - 230400
   - 460800
   - 921600

### Acceptance

- Each scenario is shell-selectable.
- Scenario transitions do not require reboot.
- `scenario stop` leaves the system in a defined operator-controlled state.

## Phase 7 — Statistics and observability

### Track per UART

- transmitted bytes
- transmitted messages
- current baudrate
- enabled state
- generator uptime / active time
- dropped or failed transmissions
- last error code or error count

### Acceptance

- `stats` shows aggregate state.
- `stats <id>` shows per-UART detail.
- Statistics remain correct under concurrent generation.

## Phase 8 — Verification

### Firmware verification

- Build for `nucleo_g070rb` using only repo-local tooling and dependencies.
- Boot on hardware.
- Confirm shell on USART2/ST-LINK VCP.
- Confirm independent output on USART1/3/4 through FTDI adapters.
- Exercise runtime baud changes.
- Run every predefined scenario at least once.
- Verify shell responsiveness during high-throughput generation.
- Verify stats counters advance plausibly.

### Repository verification

- Local helper commands map to the repo-local workflow.
- CI builds the Zephyr target from a clean checkout.
- Bootstrap scripts reproduce the environment from documented prerequisites.

## Recommended design rules

1. **Never write generator traffic to USART2.**
2. **Use one shared config/state model** for shell, scenarios, and startup defaults.
3. **Prefer static allocation** for UARTs, generators, buffers, and counters.
4. **Fail explicitly** on unsupported runtime baud changes.
5. **Treat scenarios as policy**, not a second transport path.
6. **Build from explicit repo-local paths**, not implicit host discovery.

## Risks and checks

| Risk | Impact | Check |
|---|---|---|
| Root-level west workspace assumptions leak in | Fragile tooling behavior | Keep build based on explicit local env vars |
| Full upstream dependency sync is used by default | Unnecessary disk growth | Use minimal repo-local dependency clones |
| SDK still carries excess payload | Disk waste | Measure footprint and add cleanup only if justified |
| Minimal dependency set is incomplete | First build fails | Add dependencies only when build output proves they are needed |
| UART mapping is wrong | Shell/logs on wrong ports | Hardware smoke test on all four UARTs |
| Runtime baud changes are unsupported on some port | Commands appear to work but do not | Report and preserve prior config on failure |
| Generator load blocks shell | CLI becomes unusable | Stress test while issuing shell commands |
| TX backpressure is ignored | Unrealistic behavior and bad stats | Count failures and expose them in stats |
| CI diverges from local bootstrap | “Works locally” only | Build in CI from clean repo-local bootstrap steps |

## Immediate execution order

1. Add repo-local bootstrap scripts.
2. Create local Python venv.
3. Install minimal Zephyr SDK with ARM toolchain only.
4. Clone minimal dependencies into `third_party/`.
5. Prove a trivial `nucleo_g070rb` build.
6. Measure footprint.
7. Add cleanup script only if needed.
8. Add project skeleton files.
9. Bring up USART2 shell and USART1/3/4 TX.
10. Add UART manager.
11. Add generator threads/work items.
12. Add shell commands.
13. Add scenarios.
14. Add statistics.
15. Update CI and local helper commands.

## Definition of done

Done means:

- The repo bootstraps its own Zephyr environment locally without touching other Zephyr installations.
- The repo builds a Zephyr firmware for **NUCLEO-G070RB**.
- Shell works only on **USART2 / ST-LINK VCP**.
- Log generation works concurrently on **USART1, USART3, USART4**.
- Baudrate and generator settings are changeable at runtime.
- Scenarios and stats are implemented and usable from the shell.
- Local workflow and CI use the repo-local standalone Zephyr setup.

## Sources

- Zephyr SDK variants and setup behavior: https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html
- West workspace topologies: https://docs.zephyrproject.org/latest/develop/west/workspaces.html
- West manifests and project groups: https://docs.zephyrproject.org/latest/develop/west/manifest.html
- Zephyr modules and `ZEPHYR_MODULES`: https://docs.zephyrproject.org/latest/develop/modules.html
- Upstream Zephyr manifest reference: https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/west.yml
