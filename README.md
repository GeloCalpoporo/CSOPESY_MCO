# CSOPESY Emulator - MCO2

A command-line OS emulator with a process scheduler and a demand-paging memory
manager, built for CSOPESY (Operating Systems).

MCO2 contains **everything from MCO1** — CLI, process multiplexer, FCFS/RR
scheduler — plus memory management.

**Developers:**
- Alviar, Kelvin
- Calpoporo, Angelo
- Carlos, Miguel
- Tujan, Nio

**Last Updated:** 08-01-2026

> Full documentation — every command, the instruction set, configuration, and how
> the memory manager works — is in **[README.txt](README.txt)** (the submission copy).

---

## File Structure

```
├── CSOPESY_Emulator/
│   ├── main.cpp                Entry point
│   ├── Config.h                Shared configuration struct
│   ├── ConsoleManager.h/.cpp   Console UI and command handling
│   ├── Process.h/.cpp          Process model, instruction engine, symbol table
│   ├── Scheduler.h/.cpp        CPU tick loop and scheduling logic
│   ├── MemoryManager.h/.cpp    Demand paging, frames, backing store
│   └── config.txt              Runtime configuration file
├── .gitignore
├── CMakeLists.txt
├── README.txt                  Full documentation
└── README.md
```

---

## Requirements

- CMake >= 3.15
- C++20-capable compiler (MSVC, MinGW-w64, or Clang)
- Windows, macOS, or Linux
- No third-party libraries

---

## Build Instructions

### CLion
Open the project root in CLion - it will detect `CMakeLists.txt` and configure automatically. Hit **Run**.

### Visual Studio
Open the project root folder. Visual Studio will detect `CMakeLists.txt` and offer CMake integration. Click **Enable and set source directory**, then build normally.

### Command Line
```bash
cmake -S . -B build
cmake --build build
```

The executable is produced at `build/CSOPESY_Emulator` (or `.exe` on Windows).

> **Note:** `config.txt` is copied next to the executable by CMake after every build.
> `initialize` also searches the usual IDE build-folder layouts, so it finds
> `config.txt` whether you launch from the project root or from `cmake-build-debug/`.

---

## What's new in MCO2

| Area | Added |
|---|---|
| Memory manager | Demand paging, FIFO page replacement, backing store (`csopesy-backing-store.txt`) |
| New commands | `process-smi` and `vmstat` in the main menu |
| `screen -s` | Now takes a memory size: `screen -s <name> <mem>` |
| `screen -c` | New — run a user-supplied program: `screen -c <name> <mem> "<instructions>"` |
| Instructions | `READ(var, address)` and `WRITE(address, value)` |
| `screen -r` | Reports processes killed by a memory access violation |
| `config.txt` | `max-overall-mem`, `mem-per-frame`, `min-mem-per-proc`, `max-mem-per-proc` |

```
screen -c process2 2048 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB;
                         WRITE 0x500 varA; READ varC 0x500; PRINT(\"Result: \" + varC)"
```
prints `Result: 15`.

---

## Commands

### Main Menu

| Command | Description |
|---|---|
| `initialize` | Load `config.txt` and start the scheduler. **Must be run first.** |
| `scheduler-start` | Begin auto-generating dummy processes. |
| `scheduler-stop` | Stop generating new processes (running ones continue). |
| `screen -s <name> <mem>` | Create a process with `<mem>` bytes and attach to its screen. |
| `screen -c <name> <mem> "<instructions>"` | Create a process running a user-supplied program (1-50 instructions, semicolon-separated). |
| `screen -r <name>` | Re-attach to an existing running process. |
| `screen -ls` | List all running and finished processes with CPU utilization. |
| `report-util` | Print process table to terminal and save to `csopesy-log.txt`. |
| `process-smi` | Memory summary: used/total memory and per-process usage. |
| `vmstat` | Detailed memory and CPU tick counters, pages in/out. |
| `cls` / `clear` | Clear the screen. |
| `exit` | Exit the emulator. |

### Inside a Process Screen

| Command | Description |
|---|---|
| `process-smi` | Show process info, memory, page faults, and print logs. |
| `exit` | Return to the main menu (process keeps running in background). |

---

## Configuration (`config.txt`)

```
num-cpu             4       # Number of CPU cores [1-128]
scheduler           "rr"    # Scheduling algorithm: "rr", "fcfs"
quantum-cycles      5       # RR time slice in CPU ticks
batch-process-freq  1       # Ticks between auto-generated processes
min-ins             1000    # Minimum instructions per process
max-ins             2000    # Maximum instructions per process
delay-per-exec      0       # Busy-wait ticks between instructions (0 = none)

max-overall-mem     16384   # Total simulated memory, in bytes
mem-per-frame       256     # Bytes per frame (= bytes per page)
min-mem-per-proc    256     # Memory rolled for scheduler-start processes
max-mem-per-proc    4096
```

The four memory parameters must each be a power of 2 within `[64, 65536]` bytes.

---

## Entry Point

**File:** `CSOPESY_Emulator/main.cpp`  
**Function:** `main()`

```cpp
int main() {
    ConsoleManager console;
    console.run();
    return 0;
}
```

---

## Notes

- `initialize` must be called before any other command (except `exit`).
- `screen -r` on a finished process returns `"Process <name> not found."` — finished processes are no longer accessible.
- Process memory sizes must be a power of 2 within `[64, 65536]` bytes, otherwise the console prints `invalid memory allocation`.
- Variables live in a 64-byte symbol table segment: **max 32 uint16 variables**. Declarations past that limit are ignored.
- Variables are `uint16` — values clamp to `[0, 65535]` on overflow/underflow, no wrapping.
- Reading or writing outside a process's own memory space is an access violation: the process is shut down, and `screen -r` reports when and at which address.
- A process only executes an instruction once every page it touches is resident; a faulting instruction is restarted on a later tick.
- A process keeps its memory until it finishes; frames and backing store pages are then reclaimed.
- 1 CPU tick = 1 instruction executed per core (when `delay-per-exec` is 0). The tick rate is capped at ~1000/sec.
