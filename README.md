# CSOPESY Emulator — MCO1

A command-line OS emulator with a process scheduler, built for CSOPESY (Operating Systems).

**Developers:**
- Alviar, Kelvin
- Calpoporo, Angelo
- Carlos, Miguel
- Tujan, Nio

**Last Updated:** 06-25-2026

---

## File Structure

```
├── CSOPESY_Emulator/
│   ├── main.cpp              Entry point
│   ├── Config.h              Shared configuration struct
│   ├── ConsoleManager.h/.cpp Console UI and command handling
│   ├── Process.h/.cpp        Process model and instruction engine
│   ├── Scheduler.h/.cpp      CPU tick loop and scheduling logic
│   └── config.txt            Runtime configuration file
├── .gitignore
├── CMakeLists.txt
└── README.md
```

---

## Requirements

- CMake >= 3.15
- C++20-capable compiler (MSVC, MinGW-w64, or Clang)
- Windows, macOS, or Linux

---

## Build Instructions

### CLion
Open the project root in CLion — it will detect `CMakeLists.txt` and configure automatically. Hit **Run**.

### Visual Studio
Open the project root folder. Visual Studio will detect `CMakeLists.txt` and offer CMake integration. Click **Enable and set source directory**, then build normally.

### Command Line
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

The executable is produced at `build/CSOPESY_Emulator` (or `.exe` on Windows).

> **Note:** `config.txt` is copied next to the executable by CMake automatically. If you move the exe manually, copy `config.txt` alongside it.

---

## Running

```bash
./build/CSOPESY_Emulator
```

Once the banner appears, type commands at the `root:\>` prompt.

---

## Commands

### Main Menu

| Command | Description |
|---|---|
| `initialize` | Load `config.txt` and start the scheduler. **Must be run first.** |
| `scheduler-start` | Begin auto-generating dummy processes. |
| `scheduler-stop` | Stop generating new processes (running ones continue). |
| `screen -s <name>` | Create a new named process and attach to its screen. |
| `screen -r <name>` | Re-attach to an existing running process. |
| `screen -ls` | List all running and finished processes with CPU utilization. |
| `report-util` | Print process table to terminal and save to `csopesy-log.txt`. |
| `cls` / `clear` | Clear the screen. |
| `exit` | Exit the emulator. |

### Inside a Process Screen

| Command | Description |
|---|---|
| `process-smi` | Show process info, current instruction line, and print logs. |
| `exit` | Return to the main menu (process keeps running in background). |

---

## Configuration (`config.txt`)

```
num-cpu             4       # Number of CPU cores [1–128]
scheduler           "rr"    # Scheduling algorithm: "rr", "fcfs", "rr/fcfs", "fcfs/rr"
quantum-cycles      5       # RR time slice in CPU ticks
batch-process-freq  10      # Ticks between auto-generated processes
min-ins             1000    # Minimum instructions per process
max-ins             2000    # Maximum instructions per process
delay-per-exec      0       # Busy-wait ticks between instructions (0 = none)
```

### Hybrid Scheduler (Optional)

Use `"rr/fcfs"` or `"fcfs/rr"` as the scheduler value to enable adaptive switching. Add one or both trigger keys:

```
hybrid-switch-after-finished  15    # Switch after N processes finish
hybrid-switch-after-ticks     3000  # Switch after N CPU ticks
```

The emulator prints a message to the console when the switch occurs.

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
- Process instructions (PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR) are randomized at creation time. Only PRINT output is visible via `process-smi`.
- Variables inside processes are `uint16` — values clamp to `[0, 65535]` on overflow/underflow, no wrapping.
- 1 CPU tick = 1 instruction executed per core (when `delay-per-exec` is 0).
