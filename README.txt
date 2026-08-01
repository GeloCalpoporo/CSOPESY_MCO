CSOPESY MCO2 - Multitasking OS with Memory Management
=====================================================

Developers:
  Alviar, Kelvin
  Calpoporo, Angelo
  Carlos, Miguel
  Tujan, Nio

Last updated: 08-01-2026


ENTRY POINT
-----------
File     : CSOPESY_Emulator/main.cpp
Function : main()   ->   ConsoleManager console; console.run();


REQUIREMENTS
------------
  - CMake >= 3.15
  - A C++20 compiler (MSVC, MinGW-w64, or Clang)
  - No third-party libraries. Standard library only.


BUILD AND RUN
-------------
CLion (recommended)
  Open the "MCO2" folder as the project root. CLion detects CMakeLists.txt and
  configures automatically. Press Run.

Visual Studio
  Open the "MCO2" folder. Visual Studio detects CMakeLists.txt and offers CMake
  integration. Click "Enable and set source directory", then build and run.

Command line
  cmake -S . -B build
  cmake --build build
  ./build/CSOPESY_Emulator          (CSOPESY_Emulator.exe on Windows)

config.txt is copied next to the executable after every build. "initialize" also
searches the usual IDE build-folder layouts, so it finds config.txt whether the
program is launched from the project root or from cmake-build-debug/.

Files written at runtime, in the working directory:
  csopesy-log.txt             - written by "report-util"
  csopesy-backing-store.txt   - the backing store, readable at any time


COMMANDS - MAIN MENU
--------------------
  initialize                 Load config.txt and start the scheduler.
                             MUST be run first; only "exit" works before it.
  screen -s <name> <mem>     Create a process with <mem> bytes and attach to it.
  screen -c <name> <mem> "<instructions>"
                             Create a process running a user-supplied program.
                             1-50 instructions, separated by semicolons.
  screen -r <name>           Re-attach to a running process.
  screen -ls                 List running and finished processes, CPU utilization.
  scheduler-start            Start generating dummy processes.
  scheduler-stop             Stop generating dummy processes.
  report-util                Print the process table and save it to csopesy-log.txt.
  process-smi                Memory summary: used/total memory and per-process usage.
  vmstat                     Detailed memory and CPU tick counters, pages in/out.
  cls / clear                Clear the screen.
  exit                       Exit the emulator.

COMMANDS - INSIDE A PROCESS SCREEN
----------------------------------
  process-smi                Process info, memory, page faults, and PRINT logs.
  exit                       Return to the main menu (the process keeps running).


PROCESS INSTRUCTIONS
--------------------
  PRINT(msg)                     Print a message. Default: "Hello world from <name>!"
                                 Supports PRINT("text " + var).
  DECLARE(var, value)            Declare a uint16 variable.
  ADD(var1, var2/val, var3/val)  var1 = var2 + var3   (clamped to 65535)
  SUBTRACT(...)                  var1 = var2 - var3   (clamped to 0)
  SLEEP(X)                       Sleep X CPU ticks and relinquish the CPU.
  FOR([instructions], repeats)   Loop. Nestable up to 3 levels.
  READ(var, address)             Read a uint16 from memory into var.
  WRITE(address, value/var)      Write a uint16 to memory.

Addresses are hexadecimal ("0x500") or decimal. Example, straight from the spec:

  screen -c process2 2048 "DECLARE varA 10; DECLARE varB 5; ADD varA varA varB;
                           WRITE 0x500 varA; READ varC 0x500; PRINT(\"Result: \" + varC)"

  -> prints "Result: 15"

Notes:
  - Variables live in a 64-byte symbol table segment: a maximum of 32 uint16
    variables. Declarations past that limit are ignored.
  - Reading or writing outside the process's own memory space is an access
    violation: the process is shut down, and "screen -r <name>" then reports
    "Process <name> shut down due to memory access violation error that occurred
    at <HH:MM:SS>. <hex address> invalid."
  - Process memory sizes must be a power of 2 within [64, 65536] bytes, otherwise
    the console prints "invalid memory allocation".


CONFIGURATION (config.txt)
--------------------------
  num-cpu             4       Number of CPU cores [1-128]
  scheduler           "rr"    "rr" or "fcfs"
  quantum-cycles      5       RR time slice, in CPU ticks
  batch-process-freq  1       Ticks between generated processes
  min-ins             1000    Minimum instructions per process
  max-ins             2000    Maximum instructions per process
  delay-per-exec      0       Busy-wait ticks between instructions

  max-overall-mem     16384   Total simulated memory, in bytes
  mem-per-frame       256     Bytes per frame (= bytes per page)
  min-mem-per-proc    256     Memory rolled for scheduler-start processes
  max-mem-per-proc    4096

The four memory parameters must each be a power of 2 within [64, 65536] bytes.


MEMORY MANAGEMENT (how it works)
--------------------------------
Demand paging. A process starts with NO pages resident. The first time it touches
its symbol table or a READ/WRITE address, a page fault occurs and the page is
loaded into a free frame. When no frame is free, a FIFO victim is evicted to
csopesy-backing-store.txt and its owner loses residency.

Per the spec, an instruction only runs once every page it touches is resident: a
faulting instruction is NOT executed and NOT advanced, and is restarted on a later
tick. Memory is allocated and page faults are handled only while the process holds
a CPU core. A process keeps its memory until it finishes; its frames and backing
store pages are then reclaimed.

One CPU tick executes one instruction per busy core. The tick rate is capped at
about 1000 ticks per second so that "scheduler-start" generates processes at a
demonstrable rate rather than exhausting memory.
