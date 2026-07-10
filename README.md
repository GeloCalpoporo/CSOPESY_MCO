# CSOPESY Emulator — First-Fit Memory Allocator + Round-Robin Scheduler

## Build
```
g++ -std=c++17 -O2 -pthread main.cpp -o emulator
```

## Run
```
./emulator
```
Make sure `config.txt` is in the same directory as the executable — it already
contains the required parameters:

```
num-cpu 2
scheduler rr
quantum-cycles 4
batch-process-freq 1
min-ins 100
max-ins 100
max-overall-mem 16384
mem-per-frame 16
mem-per-proc 4096
```

## Commands
| Command | Effect |
|---|---|
| `initialize` | loads `config.txt`, sets up cores + memory frames (run this first) |
| `scheduler-start` | begins auto-generating processes (1 every cycle) and dispatching them RR |
| `scheduler-stop` | stops generating *new* processes; already-created ones keep running to completion |
| `screen -ls` | shows per-core status, ready queue, finished list, memory usage |
| `screen -s <name>` | manually spawns one process |
| `exit` | quits the emulator |

## How it satisfies the spec
- **First-fit flat allocator**: memory is modeled as 1,024 frames of 16 bytes
  each (`max-overall-mem / mem-per-frame`). Each process needs 256 contiguous
  frames (`mem-per-proc / mem-per-frame`). `allocateMemory()` scans frames
  left-to-right and takes the first contiguous run that fits.
- **Fixed per-process memory**: every process always requests exactly
  `mem-per-proc` (4096) bytes.
- **Held until completion**: a process's memory is only released in
  `freeMemory()` when it finishes (reaches `totalIns` instructions) — it keeps
  its frames across quantum preemptions.
- **No backing store**: if `allocateMemory()` fails (memory full), the process
  is pushed to the tail of the ready queue and retried later — see the
  dispatch loop in `engineLoop()`.
- **Snapshot every quantum-cycles**: `writeSnapshot()` is called whenever
  `cycleCount % quantum-cycles == 0`, producing `memory_stamp_<qq>.txt` with
  timestamp, process count in memory, external fragmentation (sum of free
  bytes), and an ASCII map of address boundaries — formatted to match the
  provided mockup.
- Since `max-overall-mem / mem-per-proc = 4`, at most 4 processes ever hold
  memory simultaneously, so with more than 4 processes queued you'll see
  several finish while others wait — matching the expected result.

## Recording the required test case
1. Compile as above, open a terminal with a large, readable font.
2. Start screen/OBS recording (480p–720p, seamless, no cuts).
3. Run `./emulator`, press Enter/Run from your IDE if using one.
4. Type `initialize`.
5. Type `scheduler-start`.
6. Wait 5 seconds (visibly, on camera).
7. Type `scheduler-stop`.
8. Type `screen -ls` every ~2 seconds and let it keep running until every
   process has moved into "Finished processes" (or until ~1 minute has
   elapsed if it hasn't finished by then).
9. Type `exit`.
10. Stop recording. Do **not** touch the source code at any point during the
    recording (steps 3–9).

## Packaging your submission
After the run, collect the generated `memory_stamp_*.txt` files (they're
written into the working directory the emulator was run from) into a ZIP,
together with your screen recording, per the assignment's upload instructions.
