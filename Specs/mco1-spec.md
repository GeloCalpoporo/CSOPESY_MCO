# MCO1 spec digest — Process Scheduler and CLI

Digest of `mco1.pdf` (6 pp., 100 pts, by Neil Patrick Del Gallego, PhD; spec updated May 7, 2025).
Read this instead of the PDF. Open the PDF only to confirm **exact wording or a screen mockup**.

**Scope:** "The first part of your emulator is the process multiplexer and your command-line
interpreter (CLI)." Design reference: Linux/PowerShell shells + the Linux `screen` command.

---

## Requirement checklist (all must be implemented)

### 1. Main menu console

Banner + `root:\>` prompt. Recognizes:

| Command | Behavior |
|---|---|
| `initialize` | Loads processor config from `config.txt`. **Must be called before any other command except `exit`.** |
| `screen` | See §2. |
| `scheduler-start` | Continuously generates a batch of dummy processes for the CPU scheduler. Each is reachable via `screen`. |
| `scheduler-stop` | Stops generating dummy processes (already-running ones continue). |
| `report-util` | Generates the CPU utilization report. See §4. |
| `exit` | Terminates the console. |

`scheduler-start` and `scheduler-stop` are **main-menu only**.

### 2. `screen` command support

| Form | Behavior |
|---|---|
| `screen -s <name>` | Create a new process, clear the console, and move into its screen. |
| `screen -r <name>` | Re-attach to a running process. If not found **or already finished**, print exactly: `Process <process name> not found.` |
| `screen -ls` | List all running processes (see §4 for the format). |

Inside a process screen:

- `process-smi` — prints process name, ID, logs (from PRINT instructions), current instruction line,
  and lines of code. Re-typing it shows updated progress. If the process has finished, print
  `Finished!` after the name, ID, and logs.
- `exit` — returns to the main menu; the process keeps running in the background.

Once a process finishes, the user can no longer access its screen after exiting.

### 3. Barebones process instructions

Instructions are **pre-determined and randomized**, never typed by the user (MCO1 only — MCO2 adds `screen -c`).

| Instruction | Semantics |
|---|---|
| `PRINT(msg)` | Output `msg` — visible only inside the process's own screen. May print one variable, e.g. `PRINT("Value from: " + x)`. |
| `DECLARE(var, value)` | Declare a uint16 named `var` with default `value`. |
| `ADD(var1, var2/value, var3/value)` | `var1 = var2/value + var3/value`. |
| `SUBTRACT(var1, var2/value, var3/value)` | `var1 = var2/value - var3/value`. |
| `SLEEP(X)` | Sleep the process for X (uint8) CPU ticks and relinquish the CPU. |
| `FOR([instructions], repeats)` | For-loop over a set of instructions. Nestable. |

Rules:
- Undeclared variables used in ADD/SUBTRACT are **auto-declared with value 0**. uint16 literals allowed.
- Variables live in memory and are **not released until the process finishes**.
- uint16 values are **clamped** to `[0, max(uint16)]` — clamped, not wrapped.
- **Unless a test case says otherwise, PRINT must output exactly `Hello world from <process_name>!`**
- FOR loops nest **up to 3 levels**.

### 4. `screen -ls` and `report-util`

Identical output; `report-util` additionally writes it to `csopesy-log.txt`. Must show:

- CPU utilization %, cores used, cores available
- **Running processes:** name, `(MM/DD/YYYY HH:MM:SSAM)`, `Core: <n>`, `<current line> / <total lines>`
- **Finished processes:** name, timestamp, `Finished`, `<total> / <total>`

Both finished and running processes must appear — this is how correctness is validated.
`report-util` prints a confirmation such as `Report generated at C:/csopesy-log.txt!`.

Auto-generated process names must be human-readable and screen-addressable, e.g. `p01, p02, …, p1240`.

### 5. The scheduler

- Real-time: keeps scheduling as long as the console is alive.
- Algorithm chosen via `config.txt` + `initialize`.
- **CPU tick** = an integer counter of frame passes:
  ```cpp
  int cpuCycles = 0;
  while (OS is running) { cpuCycles++; }
  ```
- `scheduler-start`: every X CPU ticks (`batch-process-freq`) a new process enters the ready queue.
  As long as cores are free, each runs and is reachable via `screen`.

### 6. `config.txt` parameters (space-separated)

| Parameter | Meaning | Range |
|---|---|---|
| `num-cpu` | Number of CPUs | `[1, 128]` |
| `scheduler` | `"fcfs"` or `"rr"` | — |
| `quantum-cycles` | RR time slice; ignored by other schedulers | `[1, 2^32]` |
| `batch-process-freq` | Ticks between generated processes; 1 = one per cycle | `[1, 2^32]` |
| `min-ins` | Minimum instructions per process | `[1, 2^32]` |
| `max-ins` | Maximum instructions per process | `[1, 2^32]` |
| `delay-per-exec` | Busy-wait ticks before the next instruction; process stays on the CPU. 0 = one instruction per cycle | `[0, 2^32]` |

Spec defaults: `num-cpu 4`, `scheduler "rr"`, `quantum-cycles 5`, `batch-process-freq 1`,
`min-ins 1000`, `max-ins 2000`, `delay-per-exec 0`.

---

## Assessment & submission

- **Black-box quiz, time-pressured.** Only `config.txt` parameters may be changed per test case —
  **no recompiling, no source edits** during the quiz. Design accordingly.
- Each test case is demonstrated in a submitted **.MP4 video**. Some require PowerPoint.
- Deliverables: **SOURCE** (code + `README.txt` with names, run instructions, entry class/file —
  a GitHub link is an accepted alternative) and a **PPT** technical report covering: command
  recognition, console UI implementation, command interpreter implementation, process
  representation, scheduler implementation.
- Timeline: Week 7 mockup test case + quiz; Week 8 actual test case + quiz (dates on AnimoSpace).
- Grading per test case: **full** = passed with varying inputs and expected output; **partial** =
  failed but a workaround produces the expected output; **none** = failed with no workaround.

---

## Implementation status (as of aug. 01, 2026)

From reading the source and README — **not yet verified by running the build**.

| Requirement | Where | Status |
|---|---|---|
| Main menu + banner + `initialize` gate | `ConsoleManager.cpp:125-170` | present |
| `screen -s` / `-r` / `-ls` | `ConsoleManager::handleScreenCommand`, `handleScreenList` | present |
| `process-smi` / `exit` in process screen | `ConsoleManager.cpp:310-330` | present |
| `scheduler-start` / `scheduler-stop` | `Scheduler::startGeneration` / `stopGeneration` | present |
| `report-util` → `csopesy-log.txt` | `ConsoleManager::handleReportUtil` | present (log file exists) |
| 6 instruction types, FOR ≤3 deep, uint16 clamp | `Process.h:20-29`, `Process.cpp` | present |
| FCFS + RR, quantum, delay-per-exec | `Scheduler.cpp` → `schedulerLoop()` | present |
| All 7 config parameters | `Config.h`, `config.txt` | present |

Not confirmed: exact PRINT text and exact `screen -ls` column formatting against the mockups on
pp. 3–4 of the PDF. Verify those before submission — they are the easiest points to lose.
