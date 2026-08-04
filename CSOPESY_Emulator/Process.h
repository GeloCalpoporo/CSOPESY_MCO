#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <random>

class MemoryManager;

//
// Process: the "dummy program" running inside the emulator.
//
// MCO1: owns an instruction stream, a program counter and a print-log history.
// MCO2: its variables no longer live in a std::unordered_map - they live in the
// process's own simulated memory, in a 64-byte symbol table segment at address 0
// (max 32 uint16 variables). Touching memory can raise a page fault, in which case
// the instruction is NOT executed and NOT advanced: it is restarted on a later tick,
// exactly as the spec describes. Touching memory outside the process's own address
// space is an access violation and kills the process.
//
class Process {

public:
    // Symbol table segment: fixed 64 bytes at address 0 => 32 uint16 variables.
    static constexpr std::size_t SYMBOL_TABLE_BYTES = 64;
    static constexpr int         MAX_VARIABLES      = 32;

    // ----- Instruction model -----
    enum class InstrType { DECLARE, ADD, SUBTRACT, PRINT, SLEEP, FOR, READ, WRITE };

    struct Instruction {
        InstrType   type = InstrType::PRINT;
        std::string a1;            // target variable (DECLARE/ADD/SUBTRACT/READ)
        std::string a2, a3;        // operands: variable name OR numeric literal
        std::string message;       // PRINT literal ("" => default greeting)
        std::string printVar;      // optional variable appended to a PRINT
        uint16_t    value = 0;     // DECLARE value / SLEEP ticks
        std::size_t address = 0;   // READ/WRITE target address
    };

    Process(const std::string& name, int pid, int totalLines, const std::string& createdAt);

    // MCO2: bind this process to the pager. Must be called before it runs.
    void attachMemory(MemoryManager* manager, std::size_t memBytes);

    // Fill this process with a randomized program (FOR unrolled, nested up to 3 levels).
    // MCO2: the mix now includes READ/WRITE so generated processes exercise memory.
    void generateInstructions(int count, unsigned seed);

    // MCO2 "screen -c": run a user-supplied program instead of a random one.
    void setProgram(const std::vector<Instruction>& program);

    // Parse a semicolon-separated instruction string. Returns false + `error` on a
    // malformed instruction. Splitting is bracket/quote aware so FOR bodies and
    // PRINT literals may contain semicolons.
    static bool parseProgram(const std::string& text,
                             std::vector<Instruction>& out,
                             std::string& error);
    // Number of top-level instructions in `text` (before FOR unrolling) - the count
    // the 1..50 limit applies to.
    static int  countTopLevel(const std::string& text);

    // Executed exactly ONCE per CPU tick by the Scheduler. No internal loop.
    void executeNextInstruction();

    bool isSleeping() const { return sleepTicksRemaining > 0; }

    // ----- Public state read by the ConsoleManager UI -----
    std::string name;
    int         pid = 0;
    int         totalLines = 0;
    std::string createdAt;
    int         currentLine = 0;
    bool        isFinished = false;
    int         coreId = -1;            // assigned by the Scheduler

    // MCO2 state
    std::size_t        memoryBytes  = 0;
    unsigned long long pageFaults   = 0;
    // True when the last tick could not retire an instruction because a page it
    // needed was not resident and no frame could be spared. The scheduler reads this
    // so CPU utilization reports work actually done, not merely cores occupied.
    bool               stalledOnMemory = false;

    // True between a serviced page fault and the restarted instruction that consumes it.
    // The pages stay pinned across that tick boundary, otherwise the other process evicts
    // them before the restart and neither ever executes. Scheduler calls releasePages()
    // whenever the process gives up its core so the pins cannot outlive it.
    bool               holdingPages    = false;
    void               releasePages();
    bool               violated     = false;   // killed by an access violation
    std::string        violationTime;          // "HH:MM:SS"
    std::string        violationAddress;       // "0x500"

    std::mutex               logMutex;  // guards printLogs (UI reads, CPU writes)
    std::vector<std::string> printLogs;

private:
    MemoryManager* mm = nullptr;

    std::unordered_map<std::string, int> varSlot;   // variable name -> symbol table slot
    int                      nextSlot = 0;          // slots handed out so far (<= 32)
    std::vector<Instruction> instructions;          // flattened stream
    int sleepTicksRemaining = 0;

    // Paging helpers. ensurePages returns false when a fault was raised, meaning the
    // caller must restart the instruction on a later tick.
    bool ensurePages(const Instruction& ins);
    bool inRange(std::size_t addr) const;           // addr and addr+1 inside our space
    void raiseViolation(std::size_t addr);

    void     executeOne(const Instruction& ins);
    uint16_t resolve(const std::string& token);     // literal or variable (auto-declares to 0)
    void     setVariable(const std::string& name, uint16_t value);
    int      slotFor(const std::string& name, bool createIfMissing);
    static uint16_t clampU16(long v);               // clamp to [0, 65535]
    std::string timestamp() const;
    static std::string clockTime();                 // "HH:MM:SS" for violations

    // generation: appends flattened instructions until instructions.size() >= target
    void genFlat(int target, int depth, std::mt19937& rng);
};
