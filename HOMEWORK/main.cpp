// CSOPESY-style OS Emulator
// Round-robin CPU scheduler + first-fit flat memory allocator
// Build: g++ -std=c++17 -O2 -pthread main.cpp -o emulator
// Run:   ./emulator

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <random>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

using namespace std;
using namespace std::chrono;

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
struct Config {
    int numCpu = 2;
    string scheduler = "rr";
    int quantumCycles = 4;
    int batchProcessFreq = 1;
    int minIns = 100;
    int maxIns = 100;
    int delaysPerExec = 0;
    long maxOverallMem = 16384;
    int memPerFrame = 16;
    long memPerProc = 4096;
};

Config cfg;
bool initialized = false;

// ---------------------------------------------------------------------------
// Process model
// ---------------------------------------------------------------------------
enum class ProcState { NEW, READY, RUNNING, FINISHED };

struct Process {
    int pid;
    string name;
    int totalIns;
    int executed = 0;
    ProcState state = ProcState::NEW;
    bool hasMemory = false;
    int coreId = -1;
    string createdAt;
    string finishedAt;
};

// ---------------------------------------------------------------------------
// Global scheduler / memory state (protected by mtx)
// ---------------------------------------------------------------------------
mutex mtx;
vector<Process> processes;          // index by pid-1
deque<int> readyQueue;              // pids waiting for CPU (may or may not have memory yet)
vector<int> finishedList;           // pids
vector<int> coreProc;               // pid running on core, -1 = idle
vector<int> coreQuantumLeft;        // remaining quantum ticks for that core's process
vector<int> memFrames;              // -1 = free, else owner pid
int numFrames = 0;
int framesPerProc = 0;
int nextPid = 1;

atomic<bool> schedulerRunning{false};
atomic<bool> systemRunning{true};
atomic<long long> cycleCount{0};

const int TICK_MS = 100; // simulated cpu cycle duration

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
string nowStamp() {
    time_t t = time(nullptr);
    tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    ostringstream oss;
    oss << put_time(&lt, "%m/%d/%Y %I:%M:%S%p");
    return oss.str();
}

vector<string> splitArgs(const string& s) {
    istringstream iss(s);
    vector<string> out;
    string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

// ---------------------------------------------------------------------------
// Memory manager (first-fit, flat, frame-based)
// ---------------------------------------------------------------------------
// NOTE: caller must hold mtx
long allocateMemory(int pid) {
    int need = framesPerProc;
    int run = 0, start = -1;
    for (int i = 0; i < numFrames; i++) {
        if (memFrames[i] == -1) {
            if (run == 0) start = i;
            run++;
            if (run == need) {
                for (int j = start; j < start + need; j++) memFrames[j] = pid;
                return start;
            }
        } else {
            run = 0;
        }
    }
    return -1; // no contiguous block found (first-fit failed)
}

void freeMemory(int pid) {
    for (int i = 0; i < numFrames; i++)
        if (memFrames[i] == pid) memFrames[i] = -1;
}

int processesInMemory() {
    set<int> s;
    for (int f : memFrames) if (f != -1) s.insert(f);
    return (int)s.size();
}

long externalFragmentationBytes() {
    long freeFrames = 0;
    for (int f : memFrames) if (f == -1) freeFrames++;
    return freeFrames * (long)cfg.memPerFrame;
}

// caller must hold mtx
void writeSnapshot(long qq) {
    // ensure output folder exists (created once)
    static bool dirMade = false;
    if (!dirMade) {
#ifdef _WIN32
        _mkdir("memory_stamps");
#else
        mkdir("memory_stamps", 0755);
#endif
        dirMade = true;
    }
    ostringstream out;
    out << "Timestamp: (" << nowStamp() << ")\n";
    out << "Number of processes in memory: " << processesInMemory() << "\n";
    out << "Total external fragmentation in KB: " << externalFragmentationBytes() << "\n\n";
    out << "----end---- = " << cfg.maxOverallMem << "\n\n";

    int i = numFrames - 1;
    while (i >= 0) {
        int owner = memFrames[i];
        int j = i;
        while (j >= 0 && memFrames[j] == owner) j--;
        long highAddr = (long)(i + 1) * cfg.memPerFrame;
        long lowAddr = (long)(j + 1) * cfg.memPerFrame;
        if (owner != -1) {
            out << highAddr << "\n";
            out << "P" << owner << "\n";
            out << lowAddr << "\n\n";
        }
        i = j;
    }
    out << "----start----- = 0\n";

    ofstream f("memory_stamps/memory_stamp_" + to_string(qq) + ".txt");
    f << out.str();
    f.close();
}

// ---------------------------------------------------------------------------
// Process creation
// ---------------------------------------------------------------------------
// caller must hold mtx
int createProcess(const string& forcedName = "") {
    int pid = nextPid++;
    Process p;
    p.pid = pid;
    p.name = forcedName.empty() ? ("p" + to_string(pid)) : forcedName;
    p.totalIns = cfg.minIns; // min-ins == max-ins == 100 per spec; kept generic below
    if (cfg.maxIns > cfg.minIns) {
        static mt19937 rng(random_device{}());
        uniform_int_distribution<int> dist(cfg.minIns, cfg.maxIns);
        p.totalIns = dist(rng);
    }
    p.state = ProcState::READY;
    p.createdAt = nowStamp();
    processes.push_back(p);
    readyQueue.push_back(pid);
    return pid;
}

// ---------------------------------------------------------------------------
// Engine thread: one iteration = one CPU cycle
// ---------------------------------------------------------------------------
void engineLoop() {
    while (systemRunning) {
        this_thread::sleep_for(milliseconds(TICK_MS));
        lock_guard<mutex> lock(mtx);

        if (!initialized) continue; // nothing to do yet, wait for 'initialize'

        cycleCount++;

        // 1) generate new processes (only while scheduler running)
        if (schedulerRunning && cfg.batchProcessFreq > 0 &&
            cycleCount % cfg.batchProcessFreq == 0) {
            createProcess();
        }

        // 2) dispatch: fill idle cores from ready queue (first-fit memory, FCFS pick order)
        for (int c = 0; c < cfg.numCpu; c++) {
            if (coreProc[c] != -1) continue; // core busy
            int attempts = (int)readyQueue.size();
            while (attempts-- > 0 && coreProc[c] == -1) {
                if (readyQueue.empty()) break;
                int pid = readyQueue.front();
                readyQueue.pop_front();
                Process& p = processes[pid - 1];

                if (!p.hasMemory) {
                    long addr = allocateMemory(pid);
                    if (addr == -1) {
                        // memory full -> revert to tail of ready queue, try next process
                        readyQueue.push_back(pid);
                        continue;
                    }
                    p.hasMemory = true;
                }
                // assign to this core
                coreProc[c] = pid;
                coreQuantumLeft[c] = cfg.quantumCycles;
                p.state = ProcState::RUNNING;
                p.coreId = c;
                break;
            }
        }

        // 3) execute one instruction per busy core
        for (int c = 0; c < cfg.numCpu; c++) {
            int pid = coreProc[c];
            if (pid == -1) continue;
            Process& p = processes[pid - 1];
            p.executed++;
            coreQuantumLeft[c]--;

            if (p.executed >= p.totalIns) {
                p.state = ProcState::FINISHED;
                p.finishedAt = nowStamp();
                freeMemory(pid);
                p.hasMemory = false;
                finishedList.push_back(pid);
                coreProc[c] = -1;
            } else if (coreQuantumLeft[c] <= 0) {
                p.state = ProcState::READY;
                p.coreId = -1;
                readyQueue.push_back(pid);
                coreProc[c] = -1;
            }
        }

        // 4) snapshot every quantum-cycles ticks (only once emulator is active)
        static bool everStarted = false;
        if (schedulerRunning) everStarted = true;
        if (initialized && everStarted &&
            cfg.quantumCycles > 0 && cycleCount % cfg.quantumCycles == 0) {
            writeSnapshot(cycleCount / cfg.quantumCycles);
        }
    }
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------
bool loadConfig(const string& path) {
    ifstream f(path);
    if (!f.is_open()) return false;
    string key;
    while (f >> key) {
        if (key == "num-cpu") f >> cfg.numCpu;
        else if (key == "scheduler") f >> cfg.scheduler;
        else if (key == "quantum-cycles") f >> cfg.quantumCycles;
        else if (key == "batch-process-freq") f >> cfg.batchProcessFreq;
        else if (key == "min-ins") f >> cfg.minIns;
        else if (key == "max-ins") f >> cfg.maxIns;
        else if (key == "delays-per-exec") f >> cfg.delaysPerExec;
        else if (key == "max-overall-mem") f >> cfg.maxOverallMem;
        else if (key == "mem-per-frame") f >> cfg.memPerFrame;
        else if (key == "mem-per-proc") f >> cfg.memPerProc;
        else { string dummy; f >> dummy; }
    }
    return true;
}

void cmdInitialize() {
    if (!loadConfig("config.txt")) {
        cout << "config.txt not found in current directory.\n";
        return;
    }
    lock_guard<mutex> lock(mtx);
    numFrames = (int)(cfg.maxOverallMem / cfg.memPerFrame);
    framesPerProc = (int)(cfg.memPerProc / cfg.memPerFrame);
    memFrames.assign(numFrames, -1);
    coreProc.assign(cfg.numCpu, -1);
    coreQuantumLeft.assign(cfg.numCpu, 0);
    initialized = true;
    cout << "Initialized. num-cpu=" << cfg.numCpu << " scheduler=" << cfg.scheduler
         << " quantum-cycles=" << cfg.quantumCycles << " batch-process-freq=" << cfg.batchProcessFreq
         << " min-ins=" << cfg.minIns << " max-ins=" << cfg.maxIns
         << " max-overall-mem=" << cfg.maxOverallMem << " mem-per-frame=" << cfg.memPerFrame
         << " mem-per-proc=" << cfg.memPerProc << "\n";
}

void cmdSchedulerStart() {
    if (!initialized) { cout << "Run 'initialize' first.\n"; return; }
    schedulerRunning = true;
    cout << "Scheduler started.\n";
}

void cmdSchedulerStop() {
    schedulerRunning = false;
    cout << "Scheduler stopped (existing processes continue to run to completion).\n";
}

void cmdScreenLs() {
    lock_guard<mutex> lock(mtx);
    cout << "-------------------------------------------\n";
    cout << "CPU utilization:\n";
    int busy = 0;
    for (int c = 0; c < cfg.numCpu; c++) {
        cout << "  Core " << c << ": ";
        if (coreProc[c] == -1) {
            cout << "idle\n";
        } else {
            busy++;
            Process& p = processes[coreProc[c] - 1];
            cout << p.name << " (pid " << p.pid << ") "
                 << p.executed << "/" << p.totalIns << " ins, "
                 << "quantum-left=" << coreQuantumLeft[c] << "\n";
        }
    }
    cout << "Cores busy: " << busy << "/" << cfg.numCpu << "\n\n";

    cout << "Ready queue (" << readyQueue.size() << "):\n";
    for (int pid : readyQueue) {
        Process& p = processes[pid - 1];
        cout << "  " << p.name << " (pid " << p.pid << ") "
             << p.executed << "/" << p.totalIns
             << (p.hasMemory ? " [has memory]" : " [waiting for memory]") << "\n";
    }
    cout << "\nFinished processes (" << finishedList.size() << "):\n";
    for (int pid : finishedList) {
        Process& p = processes[pid - 1];
        cout << "  " << p.name << " (pid " << p.pid << ") finished at " << p.finishedAt << "\n";
    }
    cout << "\nProcesses currently in memory: " << processesInMemory()
         << " | External fragmentation: " << externalFragmentationBytes() << " bytes\n";
    cout << "-------------------------------------------\n";
}

void cmdScreenS(const string& name) {
    if (!initialized) { cout << "Run 'initialize' first.\n"; return; }
    lock_guard<mutex> lock(mtx);
    int pid = createProcess(name);
    cout << "Created process " << name << " (pid " << pid << ")\n";
}

void printHelp() {
    cout << "Commands:\n"
         << "  initialize          load config.txt and set up the emulator\n"
         << "  scheduler-start     begin generating & scheduling processes\n"
         << "  scheduler-stop      stop generating new processes\n"
         << "  screen -ls          show core/ready/finished status\n"
         << "  screen -s <name>    manually create a process\n"
         << "  exit                quit the emulator\n";
}

// ---------------------------------------------------------------------------
int main() {
    cout << "CSOPESY Emulator - type 'initialize' to begin, 'exit' to quit.\n";
    thread engine(engineLoop);

    string line;
    while (systemRunning) {
        cout << "> ";
        if (!getline(cin, line)) break;
        auto args = splitArgs(line);
        if (args.empty()) continue;

        if (args[0] == "initialize") cmdInitialize();
        else if (args[0] == "scheduler-start") cmdSchedulerStart();
        else if (args[0] == "scheduler-stop") cmdSchedulerStop();
        else if (args[0] == "screen" && args.size() >= 2 && args[1] == "-ls") cmdScreenLs();
        else if (args[0] == "screen" && args.size() >= 3 && args[1] == "-s") cmdScreenS(args[2]);
        else if (args[0] == "help") printHelp();
        else if (args[0] == "exit") { systemRunning = false; }
        else cout << "Unknown command. Type 'help'.\n";
    }

    systemRunning = false;
    engine.join();
    cout << "Emulator terminated.\n";
    return 0;
}