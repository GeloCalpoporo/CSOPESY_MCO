#include "Scheduler.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace {
    // Duration of one simulated CPU tick. Small enough to feel live, large
    // enough not to peg a core. Tune this if processes run too fast/slow.
    constexpr std::chrono::milliseconds TICK_DURATION{2};

    std::string nowTimestamp() {
        std::time_t now = std::time(nullptr);
        std::tm tm_info{};
#ifdef _WIN32
        localtime_s(&tm_info, &now);
#else
        localtime_r(&now, &tm_info);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M:%S%p", &tm_info);
        return std::string(buf);
    }
}

Scheduler::~Scheduler() {
    threadRunning = false;
    if (schedulerThread.joinable())
        schedulerThread.join();
}

// ----- initialize: load config, start the background CPU-tick thread -----
void Scheduler::initialize(const Config& cfg) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        config        = cfg;
        numCores      = cfg.num_cpu;
        if (numCores < 1)   numCores = 1;        // clamp [1, 128]
        if (numCores > 128) numCores = 128;
        useRoundRobin = (cfg.scheduler == "rr");

        if (cores.empty())
            cores.resize(numCores);
    }

    // Spawn the tick thread exactly once, even if initialize is run again.
    if (!threadRunning.exchange(true)) {
        schedulerThread = std::thread(&Scheduler::schedulerLoop, this);
    }
}

void Scheduler::startGeneration() { generating = true; }
void Scheduler::stopGeneration()  { generating = false; }

// ----- process construction helpers (caller holds mtx) -----
int Scheduler::randomInstructionCount() {
    unsigned int lo = config.min_ins, hi = config.max_ins;
    if (hi < lo) std::swap(lo, hi);
    std::uniform_int_distribution<unsigned long long> dist(0ULL,
        static_cast<unsigned long long>(hi) - lo);
    unsigned long long c = static_cast<unsigned long long>(lo) + dist(rng);
    if (c < 1)             c = 1;
    if (c > 1000000ULL)    c = 1000000ULL;       // sanity cap for the simulation
    return static_cast<int>(c);
}

std::shared_ptr<Process> Scheduler::makeProcess(const std::string& name) {
    auto p = std::make_shared<Process>(name, nextPid++, 0, nowTimestamp());
    p->generateInstructions(randomInstructionCount(), rng());   // calls Role 2's engine
    // Initial core hint so a freshly-created (not-yet-run) process never shows -1.
    p->coreId = (p->pid - 1) % numCores;
    processes.push_back(p);
    return p;
}

std::shared_ptr<Process> Scheduler::findAny(const std::string& name) {
    for (auto& p : processes)
        if (p->name == name) return p;
    return nullptr;
}

std::string Scheduler::nextProcessName() {
    ++nameSeq;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "p%02d", nameSeq);
    return std::string(buf);
}

// ----- public creation / lookup -----
std::shared_ptr<Process> Scheduler::createProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx);
    if (findAny(name) != nullptr) return nullptr;   // no duplicate names
    auto p = makeProcess(name);
    readyQueue.push_back(p);
    return p;
}

std::shared_ptr<Process> Scheduler::findProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& p : processes)
        if (p->name == name && !p->isFinished) return p;
    return nullptr;
}

std::vector<std::shared_ptr<Process>> Scheduler::getAllProcesses() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::shared_ptr<Process>> result;

    // On a core right now
    for (const auto& c : cores)
        if (c.proc && !c.proc->isFinished)
            result.push_back(c.proc);

    // Waiting in the ready queue
    for (const auto& p : readyQueue)
        if (!p->isFinished)
            result.push_back(p);

    // Parked/sleeping (relinquished CPU but not done)
    for (const auto& p : sleeping)
        if (!p->isFinished)
            result.push_back(p);

    // Finished
    for (const auto& p : processes)
        if (p->isFinished)
            result.push_back(p);

    return result;
}

// ----- reporting getters -----
int Scheduler::getCoresUsed() const {
    std::lock_guard<std::mutex> lock(mtx);
    int used = 0;
    for (const auto& c : cores)
        if (c.proc) used++;
    return used;
}

int Scheduler::getCoresAvailable() const {
    return numCores - getCoresUsed();
}

double Scheduler::getCpuUtilization() const {
    if (numCores == 0) return 0.0;
    return (getCoresUsed() / static_cast<double>(numCores)) * 100.0;
}

// =====================================================================
//  The CPU tick loop - the heart of the engine. One pass = one CPU tick.
// =====================================================================
void Scheduler::schedulerLoop() {
    while (threadRunning) {
        {
            std::lock_guard<std::mutex> lock(mtx);

            // (1) Generation: every batch-process-freq ticks spawn one process.
            if (generating && config.batch_process_freq > 0
                && (cpuTicks % config.batch_process_freq == 0)) {
                std::string name = nextProcessName();
                if (findAny(name) == nullptr) {
                    auto p = makeProcess(name);
                    readyQueue.push_back(p);
                }
            }

            // (2) Advance sleeping processes off-core (SLEEP relinquished the CPU).
            for (auto it = sleeping.begin(); it != sleeping.end(); ) {
                (*it)->executeNextInstruction();        // burns one sleep tick
                if (!(*it)->isSleeping()) {
                    readyQueue.push_back(*it);          // keeps its last core number
                    it = sleeping.erase(it);
                } else {
                    ++it;
                }
            }

            // (3) Assign ready processes to idle cores.
            for (int i = 0; i < numCores; ++i) {
                if (cores[i].proc || readyQueue.empty()) continue;
                auto p = readyQueue.front();
                readyQueue.pop_front();
                if (p->isFinished) continue;
                cores[i].proc        = p;
                cores[i].quantumLeft = useRoundRobin ? static_cast<int>(config.quantum_cycles) : 0;
                cores[i].delayLeft   = 0;
                p->coreId            = i;
            }

            // (4) Run one tick on each busy core.
            for (int i = 0; i < numCores; ++i) {
                Core& c = cores[i];
                if (!c.proc) continue;
                auto p = c.proc;

                if (p->isFinished) { c.proc = nullptr; continue; }

                // delay-per-exec: busy-wait on the core, do no work this tick.
                if (c.delayLeft > 0) {
                    c.delayLeft--;
                } else {
                    p->executeNextInstruction();
                    c.delayLeft = static_cast<int>(config.delay_per_exec);
                }

                // RR time slice counts every tick the process holds the core.
                if (useRoundRobin) c.quantumLeft--;

                // Re-evaluate state after this tick. coreId is left untouched so the
                // table keeps showing the last core a process held (never -1).
                if (p->isFinished) {                      // done -> free the core
                    c.proc = nullptr;
                }
                else if (p->isSleeping()) {               // SLEEP -> relinquish core
                    sleeping.push_back(p);
                    c.proc = nullptr;
                }
                else if (useRoundRobin && c.quantumLeft <= 0) {   // quantum -> preempt
                    readyQueue.push_back(p);
                    c.proc = nullptr;
                }
            }
        } // mutex released here

        cpuTicks++;
        std::this_thread::sleep_for(TICK_DURATION);
    }
}