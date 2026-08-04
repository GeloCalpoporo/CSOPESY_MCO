#include "Scheduler.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

namespace {
    // One tick = one instruction per busy core, exactly as the spec defines a CPU cycle.
    // The tick rate is capped so that "scheduler-start" with batch-process-freq 1 spawns
    // ~1000 processes/sec instead of millions - an unbounded tick rate exhausts memory
    // within seconds of a demo. The deadline moves by a fixed period and the loop catches
    // up after an oversleep, so the average rate holds even with Windows' coarse timer.
    constexpr auto TICK_PERIOD = std::chrono::milliseconds(1);
    constexpr auto MAX_LAG     = std::chrono::milliseconds(200);

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

// ----- initialize: load config, size memory, start the background CPU-tick thread -----
void Scheduler::initialize(const Config& cfg) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        config        = cfg;
        numCores      = cfg.num_cpu;
        if (numCores < 1)   numCores = 1;        // clamp [1, 128]
        if (numCores > 128) numCores = 128;
        useRoundRobin = (cfg.scheduler == "rr");

        // A round-robin quantum of 0 would preempt before any instruction could run.
        // Configs in the wild do set quantum-cycles 0 when they mean "FCFS, quantum
        // irrelevant", so treat it as the smallest slice that makes progress.
        if (useRoundRobin && config.quantum_cycles < 1) config.quantum_cycles = 1;

        if (cores.empty())
            cores.resize(numCores);

        mem.initialize(cfg.max_overall_mem, cfg.mem_per_frame);
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

// Processes made by scheduler-start get a memory size rolled between
// min-mem-per-proc and max-mem-per-proc. Both bounds are powers of 2, and so is
// every value in between that we pick from.
std::size_t Scheduler::randomProcessMemory() {
    std::size_t lo = config.min_mem_per_proc;
    std::size_t hi = config.max_mem_per_proc;
    if (lo == 0) lo = 64;
    if (hi < lo) std::swap(lo, hi);

    std::vector<std::size_t> choices;
    for (std::size_t m = lo; m <= hi; m *= 2) {
        choices.push_back(m);
        if (m > (static_cast<std::size_t>(1) << 30)) break;   // overflow guard
    }
    if (choices.empty()) return lo;

    std::uniform_int_distribution<std::size_t> pick(0, choices.size() - 1);
    return choices[pick(rng)];
}

std::shared_ptr<Process> Scheduler::makeProcess(const std::string& name, std::size_t memBytes) {
    auto p = std::make_shared<Process>(name, nextPid++, 0, nowTimestamp());
    p->attachMemory(&mem, memBytes);                 // must precede generation: the
    p->generateInstructions(randomInstructionCount(), rng());  // address range depends on it
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

// A process keeps its memory until it finishes - this is the only place it comes back.
void Scheduler::releaseMemory(const std::shared_ptr<Process>& p) {
    if (!p) return;
    if (memReleased.insert(p->pid).second)
        mem.releaseProcess(p->pid);
}

// ----- public creation / lookup -----
std::shared_ptr<Process> Scheduler::createProcess(const std::string& name, std::size_t memBytes) {
    std::lock_guard<std::mutex> lock(mtx);
    if (findAny(name) != nullptr) return nullptr;   // no duplicate names
    auto p = makeProcess(name, memBytes);
    readyQueue.push_back(p);
    return p;
}

std::shared_ptr<Process> Scheduler::createProcess(const std::string& name, std::size_t memBytes,
                                                  const std::vector<Process::Instruction>& program) {
    std::lock_guard<std::mutex> lock(mtx);
    if (findAny(name) != nullptr) return nullptr;

    auto p = std::make_shared<Process>(name, nextPid++, 0, nowTimestamp());
    p->attachMemory(&mem, memBytes);
    p->setProgram(program);                          // user-supplied, not randomized
    p->coreId = (p->pid - 1) % numCores;
    processes.push_back(p);
    readyQueue.push_back(p);
    return p;
}

std::shared_ptr<Process> Scheduler::findProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& p : processes)
        if (p->name == name && !p->isFinished) return p;
    return nullptr;
}

std::shared_ptr<Process> Scheduler::lookupProcess(const std::string& name) {
    std::lock_guard<std::mutex> lock(mtx);
    return findAny(name);
}

std::vector<std::shared_ptr<Process>> Scheduler::getAllProcesses() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::shared_ptr<Process>> result;
    for (const auto& c : cores)                     // actively on a core
        if (c.proc && !c.proc->isFinished)
            result.push_back(c.proc);
    for (const auto& p : sleeping)                  // temporarily sleeping (still "running")
        if (!p->isFinished)
            result.push_back(p);
    for (const auto& p : readyQueue)                // waiting for a core
        if (!p->isFinished)
            result.push_back(p);
    for (const auto& p : processes)                 // finished (in creation order)
        if (p->isFinished)
            result.push_back(p);
    return result;
}

// ----- reporting getters -----
// A core counts as used only if its process is actually getting work done. A core
// holding a process that cannot retire an instruction because memory is exhausted is
// not doing useful work, and reporting it as busy would hide exactly the condition
// these statistics exist to expose: with fewer frames than running processes need,
// utilization must fall below 100%. A delay-per-exec busy-wait still counts as used -
// there the core is deliberately held by the spec's own timing scheme.
int Scheduler::getCoresUsed() const {
    std::lock_guard<std::mutex> lock(mtx);
    int used = 0;
    for (const auto& c : cores)
        if (c.proc && !c.proc->stalledOnMemory) used++;
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
    auto nextTick = std::chrono::steady_clock::now();

    while (threadRunning) {
        // Stamp the tick before any core runs: frames paged in during this tick are
        // protected from eviction until the next one.
        mem.setTick(cpuTicks);
        {
            std::lock_guard<std::mutex> lock(mtx);

            // (1) Generation: every batch-process-freq ticks spawn one process.
            if (generating && config.batch_process_freq > 0
                && (cpuTicks % config.batch_process_freq == 0)) {
                std::string name = nextProcessName();
                if (findAny(name) == nullptr) {
                    auto p = makeProcess(name, randomProcessMemory());
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

            // (3) Run one tick on each core.
            for (int i = 0; i < numCores; ++i) {
                Core& c = cores[i];

                if (!c.proc) { idleTicks++; continue; }   // vmstat: idle core-tick
                auto p = c.proc;

                if (p->isFinished) { releaseMemory(p); c.proc = nullptr; idleTicks++; continue; }

                // delay-per-exec: busy-wait on the core, do no work this tick. The
                // process keeps the core (spec calls it a busy-waiting scheme).
                if (c.delayLeft > 0) {
                    c.delayLeft--;
                    activeTicks++;                       // the core is held, not idle
                } else {
                    p->executeNextInstruction();         // exactly one instruction per tick
                    c.delayLeft = static_cast<int>(config.delay_per_exec);
                    // A tick that retired nothing because memory was exhausted is not
                    // work: vmstat counts it as idle, so idle+active still tells the
                    // truth about how much the CPU actually accomplished.
                    if (p->stalledOnMemory) idleTicks++;
                    else                    activeTicks++;
                }

                // RR time slice counts every tick the process holds the core.
                if (useRoundRobin) c.quantumLeft--;

                // Re-evaluate state after this tick. coreId is left untouched so the
                // table keeps showing the last core a process held (never -1).
                if (p->isFinished) {                      // done (or killed) -> free the core
                    releaseMemory(p);
                    c.proc = nullptr;
                }
                else if (p->isSleeping()) {               // SLEEP -> relinquish core
                    p->releasePages();                   // pins must not outlive the core
                    sleeping.push_back(p);
                    c.proc = nullptr;
                }
                else if (useRoundRobin && c.quantumLeft <= 0) {   // quantum -> preempt
                    p->releasePages();
                    readyQueue.push_back(p);
                    c.proc = nullptr;
                }
            }

            // (4) Assign ready processes to idle cores - runs AFTER executing ticks so
            //     any core vacated this tick is immediately refilled before the next
            //     snapshot.
            for (int i = 0; i < numCores; ++i) {
                if (cores[i].proc || readyQueue.empty()) continue;
                auto p = readyQueue.front();
                readyQueue.pop_front();
                if (p->isFinished) { releaseMemory(p); continue; }
                cores[i].proc        = p;
                cores[i].quantumLeft = useRoundRobin ? static_cast<int>(config.quantum_cycles) : 0;
                cores[i].delayLeft   = 0;
                p->coreId            = i;
            }
        } // mutex released here

        // Keep csopesy-backing-store.txt current without writing it every single tick.
        mem.flushBackingStore(false);

        cpuTicks++;

        // Rate-limit the tick, catching up after an oversleep so the average holds.
        nextTick += TICK_PERIOD;
        auto now = std::chrono::steady_clock::now();
        if (now < nextTick)              std::this_thread::sleep_for(nextTick - now);
        else if (now - nextTick > MAX_LAG) nextTick = now;   // stalled: resync
    }
}
