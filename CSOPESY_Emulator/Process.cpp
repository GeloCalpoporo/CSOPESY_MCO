#include "Process.h"

#include <cctype>
#include <ctime>
#include <algorithm>

// ----- Construction -----
Process::Process(const std::string& name, int pid, int totalLines, const std::string& createdAt)
    : name(name), pid(pid), totalLines(totalLines), createdAt(createdAt) {
}

// ----- One CPU tick -----
void Process::executeNextInstruction() {
    if (isFinished) return;

    // SLEEP relinquishes the CPU: burn a tick without advancing the program counter.
    if (sleepTicksRemaining > 0) {
        sleepTicksRemaining--;
        return;
    }

    // No program loaded (e.g. a process created only for a UI test):
    // fall back to simply advancing the line counter to totalLines.
    if (instructions.empty()) {
        currentLine++;
        if (currentLine >= totalLines) isFinished = true;
        return;
    }

    if (currentLine >= static_cast<int>(instructions.size())) {
        isFinished = true;
        return;
    }

    executeOne(instructions[currentLine]);
    currentLine++;
    if (currentLine >= static_cast<int>(instructions.size())) isFinished = true;
}

// ----- Instruction dispatch -----
void Process::executeOne(const Instruction& ins) {
    switch (ins.type) {
    case InstrType::DECLARE:
        variables[ins.a1] = clampU16(ins.value);
        break;

    case InstrType::ADD: {
        long r = static_cast<long>(resolve(ins.a2)) + static_cast<long>(resolve(ins.a3));
        variables[ins.a1] = clampU16(r);                       // overflow clamps to 65535
        break;
    }

    case InstrType::SUBTRACT: {
        long r = static_cast<long>(resolve(ins.a2)) - static_cast<long>(resolve(ins.a3));
        variables[ins.a1] = clampU16(r);                       // underflow clamps to 0
        break;
    }

    case InstrType::PRINT: {
        std::string msg = ins.message.empty()
            ? ("Hello world from " + name + "!")
            : ins.message;
        if (!ins.printVar.empty())
            msg += std::to_string(resolve(ins.printVar));

        std::string entry = "(" + timestamp() + ") Core:" + std::to_string(coreId)
                          + " \"" + msg + "\"";

        std::lock_guard<std::mutex> lock(logMutex);            // UI may be reading
        printLogs.push_back(entry);
        break;
    }

    case InstrType::SLEEP:
        sleepTicksRemaining = ins.value;
        break;

    case InstrType::FOR:
        // FOR is unrolled during generation; nothing to execute here.
        break;
    }
}

// ----- Operand resolution -----
// A token is either a numeric literal or a variable name. Variables that are
// referenced before declaration are auto-declared with value 0 (per spec).
uint16_t Process::resolve(const std::string& token) {
    if (!token.empty() && (std::isdigit(static_cast<unsigned char>(token[0])))) {
        try { return clampU16(std::stol(token)); } catch (...) { return 0; }
    }
    auto it = variables.find(token);
    if (it == variables.end()) {
        variables[token] = 0;
        return 0;
    }
    return it->second;
}

uint16_t Process::clampU16(long v) {
    if (v < 0)      return 0;
    if (v > 65535)  return 65535;
    return static_cast<uint16_t>(v);
}

std::string Process::timestamp() const {
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

// ----- Random program generation (FOR unrolled, nesting <= 3) -----
void Process::generateInstructions(int count, unsigned seed) {
    std::mt19937 rng(seed);
    instructions.clear();
    if (count > 0) genFlat(count, 1, rng);
    totalLines  = static_cast<int>(instructions.size());
    currentLine = 0;
    isFinished  = (totalLines == 0);
}

void Process::genFlat(int target, int depth, std::mt19937& rng) {
    static const char* vars[] = { "x", "y", "z", "a", "b" };
    const bool allowFor = depth <= 3;                 // up to 3 nested FOR levels

    std::uniform_int_distribution<int> pick(0, allowFor ? 5 : 4);
    std::uniform_int_distribution<int> vsel(0, 4);
    std::uniform_int_distribution<int> val(0, 500);
    std::uniform_int_distribution<int> slp(1, 3);
    std::uniform_int_distribution<int> rep(2, 3);

    while (static_cast<int>(instructions.size()) < target) {
        int t = pick(rng);

        if (t == 5 && allowFor) {
            // Generate a short body once, then repeat (unroll) it.
            int    r         = rep(rng);
            size_t bodyStart = instructions.size();
            int    bodyTarget = std::min(static_cast<int>(instructions.size()) + 2, target);
            genFlat(bodyTarget, depth + 1, rng);
            size_t bodyEnd = instructions.size();

            std::vector<Instruction> body(instructions.begin() + bodyStart,
                                          instructions.begin() + bodyEnd);
            for (int i = 1; i < r && static_cast<int>(instructions.size()) < target; ++i)
                for (const auto& b : body) {
                    instructions.push_back(b);
                    if (static_cast<int>(instructions.size()) >= target) break;
                }
            continue;
        }

        Instruction ins;
        switch (t) {
        case 0:
            ins.type  = InstrType::DECLARE;
            ins.a1    = vars[vsel(rng)];
            ins.value = clampU16(val(rng));
            break;
        case 1:
            ins.type = InstrType::ADD;
            ins.a1 = vars[vsel(rng)]; ins.a2 = vars[vsel(rng)]; ins.a3 = std::to_string(val(rng));
            break;
        case 2:
            ins.type = InstrType::SUBTRACT;
            ins.a1 = vars[vsel(rng)]; ins.a2 = vars[vsel(rng)]; ins.a3 = std::to_string(val(rng));
            break;
        case 3:
            ins.type = InstrType::PRINT;          // default "Hello world from <name>!"
            break;
        case 4:
            ins.type  = InstrType::SLEEP;
            ins.value = static_cast<uint16_t>(slp(rng));
            break;
        }
        instructions.push_back(ins);
    }
}