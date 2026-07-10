#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>
#include <unordered_map>
#include <random>

//
// Process: the "dummy program" running inside the emulator.
// Owns its own isolated local memory (variables), its instruction stream,
// a program counter, and a print-log history that the UI (Role 1) reads.
// The Scheduler (Role 3) drives it one instruction per CPU tick.
//
class Process {

public:
    // ----- Instruction model -----
    enum class InstrType { DECLARE, ADD, SUBTRACT, PRINT, SLEEP, FOR };

    struct Instruction {
        InstrType   type = InstrType::PRINT;
        std::string a1;            // target variable (DECLARE/ADD/SUBTRACT)
        std::string a2, a3;        // operands: variable name OR numeric literal
        std::string message;       // PRINT literal ("" => default greeting)
        std::string printVar;      // optional variable appended to a PRINT
        uint16_t    value = 0;     // DECLARE value / SLEEP ticks
    };

    // ----- Construction (signature kept for Role 1 / Role 3 compatibility) -----
    Process(const std::string& name, int pid, int totalLines, const std::string& createdAt);

    // Fill this process with a randomized program. FOR loops are unrolled
    // (nested up to 3 levels). Sets totalLines to the final instruction count.
    // Role 3 calls this with a count in [min-ins, max-ins].
    void generateInstructions(int count, unsigned seed);

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

    std::mutex               logMutex;  // guards printLogs (UI reads, CPU writes)
    std::vector<std::string> printLogs;

private:
    std::unordered_map<std::string, uint16_t> variables;  // isolated local memory
    std::vector<Instruction>                  instructions; // flattened stream
    int sleepTicksRemaining = 0;

    void     executeOne(const Instruction& ins);
    uint16_t resolve(const std::string& token);      // literal or variable (auto-declares to 0)
    static uint16_t clampU16(long v);                // clamp to [0, 65535]
    std::string timestamp() const;

    // generation: appends flattened instructions until instructions.size() >= target
    void genFlat(int target, int depth, std::mt19937& rng);
};