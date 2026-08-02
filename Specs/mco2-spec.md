# MCO2 spec digest — Multitasking OS (memory management)

Digest of `mco2.pdf` (7 pp., 100 pts, by Neil Patrick Del Gallego, PhD; spec updated June 24, 2025).
Read this instead of the PDF. Open the PDF only to confirm **exact wording or a screen mockup**.

**Scope:** "The final part is your multi-tasking OS with memory management."
**MCO2 = MCO1 + memory management + a file-system-ish backing store.** Everything in
[mco1-spec.md](mco1-spec.md) must keep working. Design reference: Linux `top`, `free`, `vmstat`.

---

## Requirement checklist (all must be implemented)

### 1. New main-menu commands

| Command | Behavior |
|---|---|
| `process-smi` | **Now also in the main menu** (MCO1 had it only inside a process screen). Summarized view of available/used memory plus the list of processes and memory occupied — like `nvidia-smi`. |
| `vmstat` | Detailed view: active/inactive processes, available/used memory, and pages. |

`process-smi` mockup (p. 4) shows: `PROCESS-SMI V01.00 Driver Version: 01.00`, `CPU-Util: 100%`,
`Memory Usage: 1245MiB / 4795MiB`, `Memory Util: 26%`, then `Running processes and memory usage:`
with one `processNN <mem>MiB` line each.

`vmstat` must report: **total memory** (in bytes), used memory, free memory, idle CPU ticks,
active CPU ticks, total CPU ticks, num paged in, num paged out.

### 2. Memory manager — demand paging

- Must support a **demand paging allocator**.
- Pages load into physical frames **on demand**. Referencing a page not in a frame → **page fault** →
  the page is brought in from the backing store into a free frame.
- If no frames are free, a **page replacement algorithm** picks a victim to evict to the backing store.
- Memory is simulated inside the program's own address space; it is **not** a 1:1 mapping of real RAM.
- Memory is pre-allocated at startup and free for any process to use.
- Must support backing-store operations under low memory: context-switching processes in and out
  by writing/reading a file.

### 3. Backing store visibility

The backing store is a **text file** readable at any time, saved as **`csopesy-backing-store.txt`**.
`vmstat` and `process-smi` are the debugging views into memory.

### 4. Per-process memory requirement

New syntax: **`screen -s <process_name> <process_memory_size>`**

- All memory ranges are `[2^6, 2^16]` bytes **and must be a power of 2**.
- Outside that range → print `invalid memory allocation` to the user.
- Minimum 64 bytes per process (needed to store variables).
- Example: `screen -s process1 256`.

### 5. Memory-access instructions

Added to the MCO1 instruction set:

| Instruction | Semantics |
|---|---|
| `READ(var, memory_address)` | Read a uint16 from memory into `var`. Uninitialized block → value is 0. |
| `WRITE(memory_address, value)` | Write a uint16 to the given address. |

Rules:
- Addresses are **hexadecimal**, e.g. `READ my_var 0x1000`, `WRITE 0x2000 42`.
- uint16 variables are clamped to `[0, max(uint16)]` and **consume 2 bytes** of memory.
- Variables live in the process's **symbol table segment**, fixed at **64 bytes → max 32 variables**.
  Once full, later instructions that declare variables are **ignored** (not an error).
- Variables are tied to the process and not released until it finishes.
- Read/write to an **invalid address** (outside the process's memory space) → **access violation**:
  the process shuts down, like a real segfault.
- READ/WRITE are now part of the randomized instruction generation used by `scheduler-start`.

### 6. User-defined instructions at creation

New syntax: **`screen -c <process_name> <process_memory_size> "<instructions>"`**

- 1–50 instructions, **semicolon-separated**. Outside that count → throw `invalid command`.
- Example from the spec:
  ```
  screen -c process2 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT(\"Result: \" + varC)"
  ```
  → declares varA=10, varB=5, varA=15, writes 15 to 0x500, reads it into varC, prints `Result: 15`.

### 7. `screen -r` gains a memory-error message

MCO1 behavior stays: not found / finished → `Process <process name> not found.`

**New for MCO2** — if the process was killed by a memory access violation, print exactly:

```
Process <process name> shut down due to memory access violation error that occurred at <HH:MM:SS>. <Hex memory address> invalid.
```

### 8. Scheduler ↔ memory interaction

- An instruction may only execute **once a valid page has been found**. Page-fault handling repeats
  until a valid page is returned, *then* the instruction runs.
- Worked example (p. 6): 3 variables = 6 bytes, fits the 64-byte symbol table → physical memory full →
  `0x500` not resident → page fault → demand pager picks a victim frame → `0x500` brought into a
  valid frame → **restart the WRITE instruction** → repeat until a valid frame is found.
- Variable declaration also page-faults if the symbol table segment isn't in physical memory.
- **Memory allocation and page-fault handling happen only when the process is assigned a CPU worker.**

### 9. `config.txt` — MCO1 parameters plus four new ones

All MCO1 parameters carry over unchanged (`num-cpu`, `scheduler`, `quantum-cycles`,
`batch-process-freq`, `min-ins`, `max-ins`, `delay-per-exec` — see [mco1-spec.md](mco1-spec.md)).

New, all in `[2^6, 2^16]` and powers of 2:

| Parameter | Meaning |
|---|---|
| `max-overall-mem` | Maximum memory available, in bytes. |
| `mem-per-frame` | Bytes per frame (= bytes per page). Total frames = `max-overall-mem / mem-per-frame`. |
| `min-mem-per-proc` | Lower bound of memory for processes created by `scheduler-start`. |
| `max-mem-per-proc` | Upper bound. For rolled memory M, pages per process P = `M / mem-per-frame`. |

---

## Assessment & submission

- Same **black-box, time-pressured quiz**: only `config.txt` parameters change per test case —
  no recompiling. Evidence per test case is a **.MP4 video**.
- Timeline: Week 12 mockup test case + quiz; Week 13 actual test case + quiz (dates on AnimoSpace).
- Deliverables: **SOURCE** (code + `README.txt` with names, run instructions, entry file; GitHub link
  accepted) and a **PPT** technical report covering: command recognition; **process representation
  with emphasis on memory representation / memory addressing**; scheduler implementation;
  **memory management — demand paging and backing store operation**.
- Grading per test case: full / partial (workaround exists) / none.

---

## Implementation status (as of aug. 01, 2026)

**Implemented** in this branch's `CSOPESY_Emulator/`. Compiled clean with `g++ -std=c++20 -Wall -Wextra` (zero
warnings) and verified by driving the CLI, not just by reading the code.

| Requirement | Where | Verified by |
|---|---|---|
| §1 `process-smi` + `vmstat` in main menu | `ConsoleManager::displaySystemSMI/displayVmstat` | both render; totals agree with the pager |
| §2 demand paging + FIFO replacement | `MemoryManager::ensureResident/takeVictimFrame` | pressure run: 2106 paged in, 2090 paged out |
| §3 backing store text file | `MemoryManager::writeStoreFile` | `csopesy-backing-store.txt` held 535 pages with real variable bytes |
| §4 `screen -s <name> <mem>` + validation | `ConsoleManager::handleScreenCreate` | 100 / 32 / 131072 each → `invalid memory allocation` |
| §5 READ/WRITE, 64B symbol table, 32 vars | `Process::executeOne`, `slotFor` | `v32=32` stored, `v33=0` ignored |
| §5 access violation kills the process | `Process::raiseViolation` | 64B process writing `0x500` → shut down |
| §6 `screen -c` 1–50 instructions | `ConsoleManager::handleScreenCustom`, `Process::parseProgram` | 51 instrs / empty / bad keyword → `invalid command` |
| §7 `screen -r` violation message | `ConsoleManager::handleScreenResume` | exact spec sentence with `HH:MM:SS` + hex address |
| §8 fault → restart the instruction | `Process::ensurePages` | faulting instruction is not advanced |
| §9 four new config params + validation | `Config.h`, `ConsoleManager::loadConfig` | non-power-of-2 rejected at `initialize` |
| Previous MCO1 features | carried over | `screen -ls`, `report-util`, FCFS + RR all still run |

**The spec's own worked example runs correctly** — `screen -c proc2 2048 "DECLARE varA 10;
DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT(\"Result: \" + varC)"`
prints `Result: 15`.

Judgement calls worth knowing before the demo: memory is reported in **bytes** (not MiB as in
the handout mockup, since the spec caps memory at 65536 bytes); **every** page load counts as
paged-in, including a first touch; FIFO was chosen as the replacement algorithm (the spec does
not name one); and the tick rate is capped at ~1000/sec.

**Verified by an 18-case black-box suite**, not by inspection: [../Tests/](../Tests/),
run with `cd Tests && ./run-tests.sh`. See [../Tests/README.md](../Tests/README.md) for what
each case proves.

Still to do: the PPT technical report, and the .MP4 test-case videos.
