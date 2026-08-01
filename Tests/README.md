# MCO2 test cases

Black-box tests. They start the emulator, type commands into it, and check what
came out — the same way the graded quiz works. There is no unit-test framework
and none is wanted; nothing here touches the emulator's internals.

Every case maps to a numbered section of the MCO2 spec, so this doubles as the
shot list for the .MP4 videos: each case is one scenario you can record.

## Running them

```bash
cd Tests
./run-tests.sh              # all cases
./run-tests.sh 03 08        # just cases starting 03 and 08
VERBOSE=1 ./run-tests.sh 01 # print the whole transcript
```

Needs `bash` and `g++` with C++20. On Windows use Git Bash, or CLion's built-in
terminal. The script builds the emulator itself — you do not need to build first.

Every run leaves `Tests/out/<case>/` behind containing `transcript.txt` (terminal
output plus any generated files), so a failure can be read afterwards and a
passing transcript can be pasted straight into the technical report.

## The cases

| # | Case | Spec | What it proves |
|---|---|---|---|
| 01 | spec-worked-example | §5, §6 | The example printed in the handout itself prints `Result: 15` |
| 02 | memory-size-validation | §4 | Only powers of 2 in [2^6, 2^16] are accepted; the rest print `invalid memory allocation` |
| 03 | access-violation | §5, §7 | An out-of-range write kills the process, and `screen -r` reports it in the spec's exact words |
| 04 | screen-r-not-found | MCO1 | A process that finished cleanly is reported as not found, not as a memory error |
| 05 | instruction-count | §6 | `screen -c` takes 1–50 instructions; 51, empty, missing and unparseable programs are rejected |
| 06 | symbol-table-limit | §5 | The 64-byte symbol table holds exactly 32 variables; the 33rd is ignored, not an error |
| 07 | vmstat-and-process-smi | §1 | Both commands work from the main menu and report every quantity the spec lists |
| 08 | demand-paging-pressure | §2, §3 | 16 frames under heavy load: real page-ins, real evictions, a readable backing store |
| 09 | mco1-round-robin | MCO1 | Round robin, `screen -ls`, and `report-util` writing `csopesy-log.txt` all still work |
| 10 | mco1-fcfs | MCO1, §9 | The other scheduler, selected by config.txt alone — no recompiling |
| 11 | config-validation | §9 | A non-power-of-2 `max-overall-mem` is refused by name, and nothing runs until it is fixed |
| 12 | page-fault-restart | §8 | A process needing 32 pages runs correctly on 4 frames — the faulting instruction restarts |
| 13 | read-write-semantics | §5 | Unwritten memory reads as 0, values round-trip, and uint16 clamping holds |
| 14 | paging-with-completion | §2, §8 | Paging under pressure while processes still finish — the anti-livelock check |
| 15 | two-processes-one-fits | §2, §3 | Two processes on a machine that holds one: both run, swapping constantly. Also covers `quantum-cycles 0` |
| 16 | single-frame-many-cores | §1, §2 | 16 cores sharing 1 frame: CPU utilization must fall below 100%. Also covers `scheduler-test` |
| 17 | oversized-address-no-size | §5, §7 | `screen -c` with no memory size, writing outside the process — accepted, then correctly killed |
| 18 | stress-32-cores | §2, §8 | 32 cores, 20 seconds, sampled every 2s: no deadlock, counters keep climbing, processes keep finishing |

Cases 12 and 14 are the important ones for the pager itself. 12 proves a single
process survives having far less memory than it asked for; 14 proves the pager
does useful work rather than just spinning. Both would pass trivially if paging
were disabled and both fail loudly if the restart logic is wrong.

Cases 15–18 are the hostile configurations: memory far smaller than demand, more
cores than frames, commands typed without their optional arguments. They exist
because a graded run is allowed to change `config.txt` to anything legal, and
these are the shapes that break a naive implementation — usually by hanging, or
by reporting 100% CPU while achieving nothing.

## Writing a case

A case is a folder under `cases/` with two files, plus an optional third:

- `cmds.txt` — what to type. `#` starts a comment. `@sleep N` pauses so the
  scheduler thread can actually run; piped input otherwise arrives faster than
  the CPU ticks and every process would still be on instruction 0.
- `expect.txt` — the assertions, one per line:

  | Syntax | Meaning |
  |---|---|
  | `+ text` | the transcript must contain this text |
  | `- text` | it must **not** contain it |
  | `~ regex` | it must match this extended regex (do not anchor with `^` — the prompt precedes output) |
  | `N 4 text` | it must contain this text on exactly 4 lines |

- `config.txt` — optional. Without it the case uses the project's own
  `CSOPESY_Emulator/config.txt`. Since the quiz allows only config changes
  between test cases, a case that needs different memory settings ships its own.

Prefer `N` over `+` when a count is what you actually mean — `+ invalid memory
allocation` would pass even if only one of four bad inputs were rejected.

## Two configs worth keeping for the demo

- `cases/14-.../config.txt` — oversubscribed memory that still completes work.
  This is the one to record for the demand-paging requirement.
- `cases/08-.../config.txt` — the same idea pushed until it thrashes. Useful for
  showing what happens when memory runs out, but note that under this load no
  process finishes, so do not use it as the main paging demo.
