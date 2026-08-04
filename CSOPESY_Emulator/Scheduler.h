#pragma once

#include <vector>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <unordered_set>
#include "Config.h"
#include "Process.h"
#include "MemoryManager.h"

//
// Scheduler: the background "operating system" engine.
// Runs one continuous CPU-tick thread that simulates num-cpu cores, schedules
// processes with FCFS or Round Robin, generates dummy processes on demand, and
// honors the delay-per-exec busy-wait.
//
// MCO2: it also owns the MemoryManager. Memory is never touched at creation time -
// pages are demanded only while a process is on a core, and a process's frames and
// backing-store pages are reclaimed the moment it finishes.
//
class Scheduler {
public:
    Scheduler() = default;
    ~Scheduler();

    // Reads config (clamps num-cpu to [1,128]), sizes memory, starts the background thread.
    void initialize(const Config& config);

    void startGeneration();   // scheduler-start: begin spawning dummy processes
    void stopGeneration();    // scheduler-stop : stop spawning, keep running the rest

    // Manual creation (screen -s <name> <mem>). Returns nullptr if the name is taken.
    std::shared_ptr<Process> createProcess(const std::string& name, std::size_t memBytes);

    // screen -c <name> <mem> "<instructions>": same, with a user-supplied program.
    std::shared_ptr<Process> createProcess(const std::string& name, std::size_t memBytes,
                                           const std::vector<Process::Instruction>& program);

    // screen -r : returns a still-running process, or nullptr if missing/finished.
    std::shared_ptr<Process> findProcess(const std::string& name);
    // Any process, finished ones included - lets screen -r report memory violations.
    std::shared_ptr<Process> lookupProcess(const std::string& name);

    // Snapshot for screen -ls / report-util / process-smi (running + finished).
    std::vector<std::shared_ptr<Process>> getAllProcesses();

    int    getCoresUsed() const;
    int    getCoresAvailable() const;
    double getCpuUtilization() const;

    // vmstat tick counters
    unsigned long long getIdleTicks()   const { return idleTicks;   }
    unsigned long long getActiveTicks() const { return activeTicks; }
    unsigned long long getTotalTicks()  const { return idleTicks + activeTicks; }

    MemoryManager& memory() { return mem; }

private:
    struct Core {
        std::shared_ptr<Process> proc;   // process on this core (nullptr = idle)
        int quantumLeft = 0;             // RR: ticks left before preemption
        int delayLeft   = 0;             // delay-per-exec busy-wait counter
    };

    void schedulerLoop();                                   // background thread body

    // helpers (caller must already hold mtx)
    std::shared_ptr<Process> makeProcess(const std::string& name, std::size_t memBytes);
    std::shared_ptr<Process> findAny(const std::string& name);
    std::string              nextProcessName();
    int                      randomInstructionCount();
    std::size_t              randomProcessMemory();         // power of 2 in [min,max]-per-proc
    void                     releaseMemory(const std::shared_ptr<Process>& p);

    Config config;
    int    numCores      = 1;
    bool   useRoundRobin = false;

    MemoryManager mem;

    std::vector<Core>                     cores;
    std::deque<std::shared_ptr<Process>>  readyQueue;
    std::vector<std::shared_ptr<Process>> sleeping;    // parked, sleeping processes
    std::vector<std::shared_ptr<Process>> processes;   // every process ever created
    std::unordered_set<int>               memReleased; // pids whose memory is reclaimed

    std::atomic<bool>               threadRunning{false};
    std::atomic<bool>               generating{false};
    std::atomic<unsigned long long> cpuTicks{0};
    std::atomic<unsigned long long> idleTicks{0};      // core-ticks with no work done
    std::atomic<unsigned long long> activeTicks{0};    // core-ticks that ran an instruction

    int nextPid = 1;
    int nameSeq = 0;

    std::thread        schedulerThread;
    mutable std::mutex mtx;                             // guards cores/queues/processes
    std::mt19937       rng{std::random_device{}()};
};
