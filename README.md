# embed-sandbox

A configurable UART log generator for the **NUCLEO-G070RB** board, built on **Zephyr RTOS**.

Generates log traffic on three independent UART interfaces to exercise a desktop log collection tool.
CLI is on USART2 (ST-LINK Virtual COM Port) only.

## Hardware

| Function   | Peripheral | Pins     | Notes                         |
|------------|------------|----------|-------------------------------|
| Shell/CLI  | USART2     | PA2/PA3  | ST-LINK Virtual COM Port      |
| Generator 1| USART1     | PC4/PC5  | External FTDI #1              |
| Generator 2| USART3     | PD8/PD9  | External FTDI #2              |
| Generator 3| USART4     | PA0/PA1  | External FTDI #3              |

## Quick start

```bash
# 1. Bootstrap the repo-local Zephyr environment
./scripts/bootstrap_zephyr.sh

# 2. Activate the environment
source scripts/setup_env.sh

# 3. Build
cmake -B build -DBOARD=nucleo_g070rb
cmake --build build
```

Or use `just`:

```bash
just bootstrap   # one-time setup
just build       # build firmware
```

## Shell commands

| Command | Description |
|---|---|
| `uart list` | List generator UARTs |
| `uart <id> baud <val>` | Set baud rate |
| `uart <id> enable\|disable` | Enable/disable UART |
| `gen start\|stop <id>` | Start/stop log generator |
| `gen interval <id> <ms>` | Set message interval |
| `gen size <id> <bytes>` | Set message size |
| `gen random <id> on\|off` | Toggle random mode |
| `scenario list` | List scenarios |
| `scenario run <name>` | Run a test scenario |
| `scenario stop` | Stop current scenario |
| `stats [id]` | Show statistics |

## Scenarios

- **continuous** — steady INFO traffic
- **burst** — rapid message burst
- **random** — random intervals and sizes
- **high-throughput** — maximum sustainable rate
- **mixed** — different config per UART
- **baud-switching** — cycles through baud rates

## Project layout

```
├── boards/nucleo_g070rb.overlay     # Devicetree overlay for USART3/4
├── src/
│   ├── main.c                       # Entry point
│   ├── uart_manager.c/h             # UART abstraction
│   ├── generator.c/h                # Log generator
│   ├── shell_commands.c/h           # CLI commands
│   ├── scenarios.c/h                # Test scenarios
│   └── statistics.c/h               # Per-UART stats
├── scripts/
│   ├── bootstrap_zephyr.sh          # One-time env setup
│   └── setup_env.sh                 # Activate env
├── third_party/                     # Repo-local dependencies
│   ├── zephyr/                      # Zephyr RTOS (pinned)
│   └── modules/hal/                 # HAL modules (minimal set)
├── toolchains/                      # SDK + Python venv
├── CMakeLists.txt
├── prj.conf
└── justfile
```

## Dependencies

The bootstrap script sets up everything locally:

- **Python venv** — `toolchains/python/.venv`
- **Zephyr SDK** (minimal + ARM toolchain) — `toolchains/zephyr-sdk-1.0.1`
- **Zephyr v4.4.1** — `third_party/zephyr`
- **CMSIS, CMSIS_6, STM32 HAL** — `third_party/modules/hal/`

No global SDK installation required. Existing Zephyr workspaces are not affected.
