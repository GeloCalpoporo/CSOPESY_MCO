# CSOPESY Major Output 2 (MO2) — Multitasking OS with Memory Management

**Branch:** `mco2_new` (working branch for MO2) — `main`/`mco1` holds MO1.

Developers:
- Alviar, Kelvin
- Calpoporo, Angelo
- Carlos, Miguel
- Tujan, Nio

Last updated: 08-04-2026

A command-line OS emulator with a process scheduler and a demand-paging memory
manager. MO2 contains everything from MO1 — CLI, process multiplexer, FCFS and
Round Robin scheduling — plus memory management.

---

## Entry Point

| File | Function |
|---|---|
| `CSOPESY_Emulator/main.cpp` | `main()` → `ConsoleManager console; console.run();` |

---

## Requirements

- CMake >= 3.15
- A C++20 compiler (MSVC, MinGW-w64, or Clang)
- No third-party libraries. Standard library only.

---

## File Structure

```
├── CSOPESY_Emulator
│   ├── Config.h
│   ├── ConsoleManager.cpp
│   ├── ConsoleManager.h
│   ├── MemoryManager.cpp
│   ├── MemoryManager.h
│   ├── Process.cpp
│   ├── Process.h
│   ├── Scheduler.cpp
│   ├── Scheduler.h
│   ├── config.txt
│   └── main.cpp
├── .gitignore
├── CMakeLists.txt
├── CMakeSettings.json
└── README.md
```

> `csopesy-backing-store.txt` and `csopesy-log.txt` are generated at runtime and are not part of the tracked source tree (see `.gitignore`).

---

## Build and Run

### CLion (recommended)
Open the project root folder. CLion detects `CMakeLists.txt` and configures automatically. Press **Run**.

### Visual Studio
Open the project root folder. Visual Studio detects `CMakeLists.txt` and offers CMake integration. Click **"Enable and set source directory"**, then build and run.

### Command line
```bash
cmake -S . -B build
cmake --build build
./build/CSOPESY_Emulator          # CSOPESY_Emulator.exe on Windows
```

`config.txt` is copied next to the executable after every build. `initialize` also searches the usual IDE build-folder layouts, so it finds `config.txt` whether the program is launched from the project root or from `cmake-build-debug/`.

**Files written at runtime**, in the working directory:
- `csopesy-log.txt` — written by `report-util`
- `csopesy-backing-store.txt` — the backing store, readable at any time

---

## What's New in MO2

| Feature | Description |
|---|---|
| Memory manager | Demand paging, **FIFO page replacement**, and a backing store file (`csopesy-backing-store.txt`) |
| New commands | `process-smi` and `vmstat` in the main menu |
| `screen -s` | Now takes a memory size: `screen -s <name> <mem>` |
| `screen -c` | New. Runs a user-supplied program |
| Instructions | `READ(var, address)` and `WRITE(address, value)` |
| `screen -r` | Reports processes killed by a memory access violation |
| `config.txt` | `max-overall-mem`, `mem-per-frame`, `min-mem-per-proc`, `max-mem-per-proc` |

---

## Commands — Main Menu

| Command | Description |
|---|---|
| `initialize` | Load `config.txt` and start the scheduler. **Must be run first**; only `exit` works before it. |
| `screen -s <name> [<mem>]` | Create a process with `<mem>` bytes and attach to it. `<mem>` is optional; without it the process gets `max-mem-per-proc` from `config.txt`. |
| `screen -c <name> [<mem>] "<instructions>"` | Create a process running a user-supplied program. 1–50 instructions, separated by semicolons. `<mem>` is optional here too, same fallback. |
| `screen -r <name>` | Re-attach to a running process. |
| `screen -ls` | List running and finished processes, CPU utilization. |
| `scheduler-start` | Start generating dummy processes. |
| `scheduler-test` | Accepted as an alias of `scheduler-start`. |
| `scheduler-stop` | Stop generating dummy processes. |
| `report-util` | Print the process table and save it to `csopesy-log.txt`. |
| `process-smi` | Memory summary: used/total memory and per-process usage. |
| `vmstat` | Detailed memory and CPU tick counters, pages in/out. |
| `cls` / `clear` | Clear the screen. |
| `exit` | Exit the emulator. |

## Commands — Inside a Process Screen

| Command | Description |
|---|---|
| `process-smi` | Process info, memory, page faults, and PRINT logs. |
| `exit` | Return to the main menu (the process keeps running). |

Any other main-menu command (`screen -ls`, `vmstat`, `scheduler-start`, ...) also works from inside a process screen and behaves exactly as it would outside, so you never have to exit a screen just to type a command.

---

## Process Instructions

| Instruction | Description |
|---|---|
| `PRINT(msg)` | Print a message. Default: `"Hello world from <name>!"`. Supports `PRINT("text " + var)`. |
| `DECLARE(var, value)` | Declare a uint16 variable. |
| `ADD(var1, var2/val, var3/val)` | `var1 = var2 + var3` (clamped to 65535) |
| `SUBTRACT(...)` | `var1 = var2 - var3` (clamped to 0) |
| `SLEEP(X)` | Sleep X CPU ticks and relinquish the CPU. |
| `FOR([instructions], repeats)` | Loop. Nestable up to 3 levels. |
| `READ(var, address)` | Read a uint16 from memory into var. |
| `WRITE(address, value/var)` | Write a uint16 to memory. |

Addresses are hexadecimal (`0x500`) or decimal. Example, straight from the spec:

```
screen -c process2 2048 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB;
                         WRITE 0x500 varA; READ varC 0x500; PRINT(\"Result: \" + varC)"
```
→ prints `"Result: 15"`

**Notes:**
- Variables live in a 64-byte symbol table segment: a maximum of 32 uint16 variables. Declarations past that limit are ignored.
- Reading or writing outside the process's own memory space is an access violation: the process is shut down, and `screen -r <name>` then reports `Process <name> shut down due to memory access violation error that occurred at <HH:MM:SS>. <hex address> invalid.`
- Process memory sizes must be a power of 2 within `[64, 65536]` bytes, otherwise the console prints `invalid memory allocation`.
- `screen -r` on a finished process returns `Process <name> not found.`

---

## Configuration (`config.txt`)

| Key | Default | Description |
|---|---|---|
| `num-cpu` | 4 | Number of CPU cores `[1-128]` |
| `scheduler` | `"rr"` | `"rr"` or `"fcfs"` |
| `quantum-cycles` | 5 | RR time slice, in CPU ticks |
| `batch-process-freq` | 1 | Ticks between generated processes |
| `min-ins` | 1000 | Minimum instructions per process |
| `max-ins` | 2000 | Maximum instructions per process |
| `delay-per-exec` | 0 | Busy-wait ticks between instructions |
| `max-overall-mem` | 16384 | Total simulated memory, in bytes |
| `mem-per-frame` | 256 | Bytes per frame (= bytes per page) |
| `min-mem-per-proc` | 256 | Memory rolled for `scheduler-start` processes |
| `max-mem-per-proc` | 4096 | |

The four memory parameters must each be a power of 2 within `[64, 65536]` bytes.

---

## Memory Management — How It Works

Demand paging. A process starts with **no pages resident**. The first time it touches its symbol table or a READ/WRITE address, a page fault occurs and the page is loaded into a free frame. When no frame is free, a **FIFO victim** is evicted to `csopesy-backing-store.txt` and its owner loses residency.

Per the spec, an instruction only runs once every page it touches is resident: a faulting instruction is **not** executed and **not** advanced, and is restarted on a later tick. Every page an instruction needs is acquired together, so an instruction needing two pages cannot end up holding one and losing it before the restart.

Memory is allocated and page faults are handled only while the process holds a CPU core. A process keeps its memory until it finishes; its frames and backing store pages are then reclaimed.

One CPU tick executes one instruction per busy core. The tick rate is capped at about 1000 ticks per second so that `scheduler-start` generates processes at a demonstrable rate rather than exhausting memory.