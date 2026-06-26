#pragma once

#include <vector>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include "Config.h"
#include "Process.h"

//
// Scheduler: the background "operating system" engine.
// Runs one continuous CPU-tick thread that simulates num-cpu cores, schedules
// processes with FCFS or Round Robin, generates dummy processes on demand, and
// honors the delay-per-exec busy-wait. The UI (Role 1) only ever calls the
// public methods below; all shared state is guarded by a single mutex.
//
class Scheduler {
public:
    Scheduler() = default;
    ~Scheduler();

    // Reads config (clamps num-cpu to [1,128]) and starts the background thread.
    void initialize(const Config& config);

    void startGeneration();   // scheduler-start: begin spawning dummy processes
    void stopGeneration();    // scheduler-stop : stop spawning, keep running the rest

    // Manual creation (screen -s). Returns nullptr if the name is already taken.
    std::shared_ptr<Process> createProcess(const std::string& name);

    // screen -r : returns a still-running process, or nullptr if missing/finished.
    std::shared_ptr<Process> findProcess(const std::string& name);

    // Snapshot for screen -ls / report-util (running + finished).
    std::vector<std::shared_ptr<Process>> getAllProcesses();

    int    getCoresUsed() const;
    int    getCoresAvailable() const;
    double getCpuUtilization() const;

private:
    struct Core {
        std::shared_ptr<Process> proc;   // process on this core (nullptr = idle)
        int quantumLeft = 0;             // RR: ticks left before preemption
        int delayLeft   = 0;             // delay-per-exec busy-wait counter
    };

    void schedulerLoop();                                   // background thread body

    // helpers (caller must already hold mtx)
    std::shared_ptr<Process> makeProcess(const std::string& name);
    std::shared_ptr<Process> findAny(const std::string& name);
    std::string              nextProcessName();
    int                      randomInstructionCount();
    void                     loadHybridConfig();   // reads optional hybrid-switch-* keys

    Config config;
    int    numCores      = 1;
    bool   useRoundRobin = false;

    std::vector<Core>                     cores;
    std::deque<std::shared_ptr<Process>>  readyQueue;
    std::vector<std::shared_ptr<Process>> sleeping;    // parked, sleeping processes
    std::vector<std::shared_ptr<Process>> processes;   // every process ever created

    std::atomic<bool>               threadRunning{false};
    std::atomic<bool>               generating{false};
    std::atomic<unsigned long long> cpuTicks{0};

    int nextPid = 1;
    int nameSeq = 0;

    // --- Adaptive hybrid scheduler (optional enhancement) ---
    // scheduler "rr/fcfs" starts in RR and switches to FCFS; "fcfs/rr" does the
    // reverse. The switch fires once a threshold below is reached. Plain "rr"
    // or "fcfs" run normally (spec behavior) and never switch.
    bool               hybridForm          = false;  // scheduler given as "a/b"
    bool               hybridEnabled       = false;
    bool               hybridTargetRR      = false;  // mode to switch TO
    bool               switched            = false;
    unsigned long long switchAfterTicks    = 0;   // 0 = disabled
    int                switchAfterFinished = 0;   // 0 = disabled
    int                finishedCount       = 0;

    std::thread        schedulerThread;
    mutable std::mutex mtx;                             // guards cores/queues/processes
    std::mt19937       rng{std::random_device{}()};
};
