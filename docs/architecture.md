# Project architecture and setup

## What this is

Firmware for the **NUCLEO-G070RB** that generates configurable log traffic on three
independent UART interfaces. Useful for testing serial log collection tools.

A CLI shell on USART2 (ST-LINK Virtual COM Port) controls the generators at runtime:
change baud rates, start/stop log streams, run predefined scenarios, and inspect
per-UART statistics.

---

## Why this Zephyr setup, not the standard one

The canonical Zephyr workflow uses `west init` + `west update` to create a workspace
with the Zephyr tree, all HAL modules, and the application living side by side.
This assumes a **workspace directory that is not itself a Git repository**.

This project is an existing Git repo with its own CI/CD. Nesting a west workspace
at the root is not supported by Zephyr's documentation — the root being a Git repo
creates problems with west's workspace discovery.

**Decision:** keep the repo root as the application repository, and manage Zephyr
dependencies manually via repo-local paths. The build uses explicit environment
variables instead of west-managed workspace discovery.

This is more work to set up but gives:
- **Isolation** — does not interfere with other Zephyr workspaces on the host
- **Self-containment** — `git clone + ./scripts/bootstrap_zephyr.sh` is all you need
- **No global state** — no host-wide SDK registration, no shared Python venv
- **CI reproducibility** — same bootstrap script works in CI

The build approach (explicit `ZEPHYR_BASE` + `ZEPHYR_MODULES` + `ZEPHYR_SDK_INSTALL_DIR`)
is directly supported by Zephyr's build system.

---

## Repository layout

```
├── boards/
│   └── nucleo_g070rb.overlay   # enables USART3 and USART4
├── docs/
│   └── stm32-zephyr-migration-plan.md
├── scripts/
│   ├── bootstrap_zephyr.sh     # one-time: SDK, Zephyr, modules
│   ├── setup_env.sh            # source this for the build environment
│   └── verify_serial.py        # CI helper — checks firmware boots
├── src/
│   ├── main.c                  # entry point, UART validation
│   ├── uart_manager.c/h        # UART device abstraction
│   ├── generator.c/h           # log generator threads
│   ├── shell_commands.c/h      # CLI command handlers
│   ├── scenarios.c/h           # predefined test scenarios
│   └── statistics.c/h          # per-UART stats counters
├── third_party/                 # auto-fetched by bootstrap (not tracked)
│   ├── zephyr/                 # Zephyr RTOS v4.4.1
│   └── modules/hal/
│       ├── cmsis/              # CMSIS headers
│       ├── hal_stm32/          # STM32 HAL drivers
│       └── CMSIS_6/            # CMSIS v6 (needed for Cortex-M)
├── toolchains/                  # auto-fetched by bootstrap (not tracked)
│   ├── python/.venv/           # repo-local Python virtual environment
│   └── zephyr-sdk-1.0.1/       # minimal SDK + ARM GCC toolchain
├── CMakeLists.txt               # Zephyr app build file
├── prj.conf                     # Kconfig configuration
├── justfile                     # developer command shortcuts
└── .gitignore                   # ignores build/, third_party/, toolchains/
```

---

## Build system

### How a build works

```
source scripts/setup_env.sh          # sets ZEPHYR_BASE, ZEPHYR_MODULES, SDK, PATH
cmake -B build -DBOARD=nucleo_g070rb  # configure
cmake --build build                   # compile + link
```

Without `source`, use the `justfile` which bakes the env vars into each recipe:

```
just build
```

### Environment variables (what setup_env.sh exports)

| Variable | Value | Purpose |
|---|---|---|
| `ZEPHYR_BASE` | `<repo>/third_party/zephyr` | Zephyr root |
| `ZEPHYR_SDK_INSTALL_DIR` | `<repo>/toolchains/zephyr-sdk-1.0.1` | SDK location |
| `ZEPHYR_TOOLCHAIN_VARIANT` | `zephyr` | Use Zephyr SDK toolchain |
| `ZEPHYR_MODULES` | `<hal paths>`; delimited | External HAL modules |
| `ZEPHYR_CMSIS_6_CMAKE_DIR` | `<ZEPHYR_BASE>/modules/cmsis_6` | CMSIS_6 integration layer |
| `PATH` | + `toolchain/bin` | ARM cross-compiler added |

### Module discovery

Zephyr's module system scans `ZEPHYR_MODULES` for `zephyr/module.yml` files.
The three modules we include are:

| Module | Provides |
|---|---|
| `modules/hal/cmsis` | Generic CMSIS (Cortex Microcontroller Software Interface Standard) |
| `modules/hal/hal_stm32` | STM32Cube HAL for all STM32 series |
| `modules/hal/CMSIS_6` | CMSIS version 6, required by Zephyr 4.4.1 Cortex-M support |

`CMSIS_6` uses `cmake-ext: true` — its CMakeLists.txt lives at
`<ZEPHYR_BASE>/modules/cmsis_6/`, which is why `ZEPHYR_CMSIS_6_CMAKE_DIR` is
set separately.

### Pinned versions

| Component | Version |
|---|---|
| Zephyr | v4.4.1 |
| Zephyr SDK | 1.0.1 (minimal bundle + ARM toolchain) |
| CMSIS | `512cc7e` |
| STM32 HAL | `ff19fb0` |
| CMSIS_6 | `30a859f` (must match exactly — other revisions lack CMakeLists.txt) |

---

## Firmware architecture

### Module responsibilities

```
main.c
  │
  ├── uart_manager_init()      # resolve USART1/3/4, validate readiness
  ├── gen_init()               # initialize generator state (all disabled)
  └── main loop                # k_sleep(K_SECONDS(1))
                                # (all work is in threads + shell callbacks)

uart_manager.c                 generator.c
  ┌─────────────────┐           ┌─────────────────────┐
  │ device handles   │           │ GEN1 ──thread──►   │
  │ per-UART config  │           │   USART1            │
  │ baud, enabled    │           │                     │
  │                  │           │ GEN2 ──thread──►   │
  │ uart_get()       │◄──────────│   USART3            │
  │ uart_set_baud()  │           │                     │
  │ uart_send()      │◄──────────│ GEN3 ──thread──►   │
  │ uart_enable()    │           │   USART4            │
  │ uart_disable()   │           └─────────────────────┘
  └─────────────────┘
        ▲                         ▲
        │ shell callbacks         │ scenario orchestrator
        │                         │
  shell_commands.c            scenarios.c
  ┌─────────────────┐           ┌─────────────────────┐
  │ uart list/baud   │           │ continuous          │
  │ gen start/stop   │           │ burst               │
  │ scenario run     │           │ random              │
  │ stats            │           │ high-throughput     │
  └─────────────────┘           │ mixed               │
        ▲                       │ baud-switching      │
        │                       └─────────────────────┘
  Zephyr shell subsystem               │
  (USART2 only)                   statistics.c
                                  ┌─────────────────────┐
                                  │ per-UART counters    │
                                  │ tx_bytes, drops,     │
                                  │ active time          │
                                  └─────────────────────┘
```

### UART separation

| UART | Purpose | In uart_manager? |
|---|---|---|
| USART2 | Zephyr console + shell | **No** — reserved by the system |
| USART1 | Generator port 1 (`UART_GEN1`) | Yes |
| USART3 | Generator port 2 (`UART_GEN2`) | Yes |
| USART4 | Generator port 3 (`UART_GEN3`) | Yes |

This is enforced by design: `uart_manager` only resolves USART1/3/4, and
`uart_disable()` cannot touch USART2.

### Generator threads

Each generator runs in its own Zephyr thread (stack: 1024 bytes, priority 5).
The thread loops:

```
gen_start(id) → thread wakes
  loop:
    if not enabled: sleep forever (k_sem_take)
    format message into stack buffer
    uart_send(id, buf, len)        # calls uart_tx()
    stats_record_tx(id, len)       # increment counters
    k_sleep(interval_ms)
```

Messages are formatted with `snprintk()` into a fixed 256-byte stack buffer.

### Shell commands

```
uart
  list                          — show all generator UARTs
  uart1 baud <val>              — set baud rate
  uart1 enable / disable        — power control
  uart3 / uart4 same

gen
  uart1 start / stop            — start/stop generator
  uart1 interval <ms>           — message period
  uart1 size <bytes>            — message length
  uart1 random on / off         — random mode toggle
  uart3 / uart4 same

scenario
  list                          — list names
  run <name>                    — execute scenario
  stop                          — stop current scenario

stats
  [id]                          — per-UART or aggregate
```

### Scenarios

| Name | Behavior |
|---|---|
| `continuous` | All UARTs: interval 1s, size 64, static text |
| `burst` | All UARTs: interval 10ms, size 128, runs 5s then stops |
| `random` | Each UART: random interval 50-549ms, random size 32-287, random payload |
| `high-throughput` | All UARTs: interval 10ms, size 256 (max) |
| `mixed` | UART1@9600 baud / interval 2s, UART3@115200 / 500ms, UART4@921600 / 100ms |
| `baud-switching` | Cycles baud 9600→921600 across all generator UARTs every 3s in a thread |

---

## Hardware map

| Pin | Function | Peripheral |
|---|---|---|
| PA2 | TX | USART2 → ST-LINK VCP (shell) |
| PA3 | RX | USART2 |
| PC4 | TX | USART1 → FTDI #1 |
| PC5 | RX | USART1 |
| PD8 | TX | USART3 → FTDI #2 |
| PD9 | RX | USART3 |
| PA0 | TX | USART4 → FTDI #3 |
| PA1 | RX | USART4 |

The board overlay at `boards/nucleo_g070rb.overlay` enables USART3 and USART4
with their pinctrl assignments. USART1 and USART2 are already enabled in the
base board DTS (`third_party/zephyr/boards/st/nucleo_g070rb/nucleo_g070rb.dts`).

---

## Boot flow

1. **Zephyr kernel init** — sets up console, shell backend on USART2
2. `main()`:
   a. `uart_manager_init()` — resolves `DEVICE_DT_GET(usart1/3/4)`, validates readiness
   b. `gen_init()` — zeroes generator state, all disabled
   c. Sends a one-line banner on each generator UART
   d. Enters idle loop (`k_sleep`)
3. **Shell prompt appears** on USART2 — user controls generators at runtime

---

## How to add something new

### New scenario

1. Add the run logic in `scenarios.c` (call `gen_set_*` / `uart_set_baud` / `gen_start`)
2. Add the name to the `scenario_list` array in `scenarios.c`
3. The shell `scenario run <name>` handles it automatically

### New UART

1. Add the devicetree enable + pinctrl in `boards/nucleo_g070rb.overlay`
2. Add an entry in `uart_manager.c` (`gen_uart_devs[]`, `gen_uart_labels[]`)
3. Extend the `uart_id` enum in `uart_manager.h`
4. Add a shell subcommand in `shell_commands.c`

### New shell command

Register with `SHELL_CMD_REGISTER()` or `SHELL_CMD_ARG_REGISTER()` in
`shell_commands.c`. See Zephyr's `shell.h` API for the callback signature.

---

## CI/CD

The workflow at `.github/workflows/build-and-flash.yml`:

1. **Build job**: bootstraps the Zephyr environment from scratch, builds firmware
2. **Flash job**: writes `zephyr.hex` via ST-LINK
3. **Verify job**: runs `scripts/verify_serial.py` to confirm the banner appears

The workflow runs on a `self-hosted` runner with the board connected.

---

## References

- Zephyr build system with explicit `ZEPHYR_MODULES`:
  https://docs.zephyrproject.org/latest/develop/modules.html
- Zephyr SDK variants and toolchain selection:
  https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html
- West workspace topology constraints:
  https://docs.zephyrproject.org/latest/develop/west/workspaces.html
- NUCLEO-G070RB board documentation:
  https://docs.zephyrproject.org/latest/boards/st/nucleo_g070rb/doc/index.html
