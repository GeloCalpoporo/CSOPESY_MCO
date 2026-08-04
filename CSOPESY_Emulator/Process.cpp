#include "Process.h"
#include "MemoryManager.h"

#include <cctype>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace {

// ---------- small text helpers used by the "screen -c" parser ----------

std::string trim(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// Split on `sep`, but only at nesting depth 0 and outside quotes, so that
// FOR([A; B], 2) and PRINT("a; b") survive intact.
std::vector<std::string> splitTop(const std::string& s, char sep) {
    std::vector<std::string> out;
    int depth = 0;
    bool inQuote = false;
    std::string cur;

    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size() && s[i + 1] == '"') { cur += '"'; ++i; continue; }
        if (c == '"')                       inQuote = !inQuote;
        else if (!inQuote && (c == '(' || c == '[')) depth++;
        else if (!inQuote && (c == ')' || c == ']')) depth--;

        if (c == sep && depth <= 0 && !inQuote) { out.push_back(cur); cur.clear(); }
        else                                      cur += c;
    }
    out.push_back(cur);
    return out;
}

// "KEYWORD rest..." or "KEYWORD(rest...)" -> keyword + the argument text
void splitKeyword(const std::string& instr, std::string& keyword, std::string& rest) {
    std::size_t i = 0;
    while (i < instr.size() && (std::isalpha(static_cast<unsigned char>(instr[i])) || instr[i] == '_')) ++i;
    keyword = upper(instr.substr(0, i));
    rest    = trim(instr.substr(i));

    if (rest.size() >= 2 && rest.front() == '(' && rest.back() == ')')
        rest = trim(rest.substr(1, rest.size() - 2));
}

// Arguments may be comma- or space-separated; both forms appear in the spec.
std::vector<std::string> splitArgs(const std::string& rest) {
    std::vector<std::string> args;
    for (const std::string& piece : splitTop(rest, ',')) {
        std::istringstream iss(piece);
        std::string tok;
        while (iss >> tok) args.push_back(tok);
    }
    args.erase(std::remove(args.begin(), args.end(), std::string()), args.end());
    return args;
}

bool isNumericToken(const std::string& t) {
    if (t.empty()) return false;
    return std::isdigit(static_cast<unsigned char>(t[0])) != 0;
}

// Accepts hexadecimal ("0x500") and plain decimal.
bool parseAddress(const std::string& tok, std::size_t& out) {
    if (tok.empty()) return false;
    try {
        std::size_t pos = 0;
        unsigned long long v =
            (tok.size() > 2 && tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X'))
                ? std::stoull(tok.substr(2), &pos, 16)
                : std::stoull(tok, &pos, 10);
        out = static_cast<std::size_t>(v);
        return true;
    } catch (...) { return false; }
}

bool parseU16(const std::string& tok, uint16_t& out) {
    try {
        long v = std::stol(tok);
        if (v < 0)     v = 0;
        if (v > 65535) v = 65535;
        out = static_cast<uint16_t>(v);
        return true;
    } catch (...) { return false; }
}

constexpr std::size_t MAX_PROGRAM_INSTRUCTIONS = 50000;   // guard against FOR blow-up

bool parseOne(const std::string& raw, std::vector<Process::Instruction>& out,
              std::string& error, int depth);

// FOR([ body ], repeats) - body is unrolled `repeats` times, nesting capped at 3.
bool parseFor(const std::string& rest, std::vector<Process::Instruction>& out,
              std::string& error, int depth) {
    if (depth > 3) { error = "FOR loops may only be nested up to 3 times"; return false; }

    std::size_t open = rest.find('[');
    if (open == std::string::npos) { error = "FOR expects FOR([instructions], repeats)"; return false; }

    int bracket = 0;
    std::size_t close = std::string::npos;
    for (std::size_t i = open; i < rest.size(); ++i) {
        if (rest[i] == '[') bracket++;
        else if (rest[i] == ']') { if (--bracket == 0) { close = i; break; } }
    }
    if (close == std::string::npos) { error = "FOR is missing a closing ']'"; return false; }

    std::string body = rest.substr(open + 1, close - open - 1);

    std::string tail = trim(rest.substr(close + 1));
    if (!tail.empty() && tail.front() == ',') tail = trim(tail.substr(1));
    long repeats = 0;
    try { repeats = std::stol(tail); } catch (...) { error = "FOR is missing a repeat count"; return false; }
    if (repeats < 0) repeats = 0;

    std::vector<Process::Instruction> bodyInstructions;
    for (const std::string& piece : splitTop(body, ';'))
        if (!parseOne(piece, bodyInstructions, error, depth + 1)) return false;

    for (long r = 0; r < repeats; ++r) {
        if (out.size() + bodyInstructions.size() > MAX_PROGRAM_INSTRUCTIONS) {
            error = "program is too large after unrolling FOR loops";
            return false;
        }
        out.insert(out.end(), bodyInstructions.begin(), bodyInstructions.end());
    }
    return true;
}

// PRINT("literal" + var) / PRINT(var) / PRINT
bool parsePrint(const std::string& rest, std::vector<Process::Instruction>& out) {
    Process::Instruction ins;
    ins.type = Process::InstrType::PRINT;

    std::string body = rest;
    std::size_t q1 = body.find('"');
    if (q1 != std::string::npos) {
        std::size_t q2 = body.rfind('"');
        if (q2 > q1) {
            ins.message = body.substr(q1 + 1, q2 - q1 - 1);
            std::string tail = trim(body.substr(q2 + 1));
            if (!tail.empty() && tail.front() == '+') ins.printVar = trim(tail.substr(1));
        }
    } else {
        std::string v = trim(body);
        if (!v.empty()) ins.printVar = v;      // PRINT(x) prints the value of x
    }
    out.push_back(ins);
    return true;
}

bool parseOne(const std::string& raw, std::vector<Process::Instruction>& out,
              std::string& error, int depth) {
    std::string instr = trim(raw);
    if (instr.empty()) return true;

    if (out.size() > MAX_PROGRAM_INSTRUCTIONS) {
        error = "program is too large after unrolling FOR loops";
        return false;
    }

    std::string keyword, rest;
    splitKeyword(instr, keyword, rest);

    if (keyword == "FOR")   return parseFor(rest, out, error, depth);
    if (keyword == "PRINT") return parsePrint(rest, out);

    std::vector<std::string> args = splitArgs(rest);
    Process::Instruction ins;

    if (keyword == "DECLARE") {
        if (args.size() < 2) { error = "DECLARE expects <var> <value>"; return false; }
        ins.type = Process::InstrType::DECLARE;
        ins.a1   = args[0];
        if (!parseU16(args[1], ins.value)) { error = "DECLARE value must be a number"; return false; }
    }
    else if (keyword == "ADD" || keyword == "SUBTRACT") {
        if (args.size() < 3) { error = keyword + " expects <var1> <var2/value> <var3/value>"; return false; }
        ins.type = (keyword == "ADD") ? Process::InstrType::ADD : Process::InstrType::SUBTRACT;
        ins.a1 = args[0]; ins.a2 = args[1]; ins.a3 = args[2];
    }
    else if (keyword == "SLEEP") {
        if (args.empty()) { error = "SLEEP expects <ticks>"; return false; }
        ins.type = Process::InstrType::SLEEP;
        uint16_t t = 0;
        if (!parseU16(args[0], t)) { error = "SLEEP expects a number"; return false; }
        ins.value = static_cast<uint16_t>(t & 0xFF);       // uint8 per spec
    }
    else if (keyword == "READ") {
        if (args.size() < 2) { error = "READ expects <var> <address>"; return false; }
        ins.type = Process::InstrType::READ;
        ins.a1   = args[0];
        if (!parseAddress(args[1], ins.address)) { error = "READ address is not a valid number"; return false; }
    }
    else if (keyword == "WRITE") {
        if (args.size() < 2) { error = "WRITE expects <address> <var/value>"; return false; }
        ins.type = Process::InstrType::WRITE;
        if (!parseAddress(args[0], ins.address)) { error = "WRITE address is not a valid number"; return false; }
        ins.a2 = args[1];
    }
    else {
        error = "unknown instruction '" + keyword + "'";
        return false;
    }

    out.push_back(ins);
    return true;
}

} // namespace

// ----- Construction -----
Process::Process(const std::string& name, int pid, int totalLines, const std::string& createdAt)
    : name(name), pid(pid), totalLines(totalLines), createdAt(createdAt) {
}

void Process::attachMemory(MemoryManager* manager, std::size_t memBytes) {
    mm          = manager;
    memoryBytes = memBytes;
    if (mm) {
        mm->registerProcess(pid, memBytes);
        mm->setProcessName(pid, name);
    }
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

    const Instruction& ins = instructions[currentLine];

    // MCO2: an instruction may only run once every page it touches is resident.
    // A fault (or an access violation) leaves currentLine untouched - the instruction
    // is restarted on a later tick.
    if (!ensurePages(ins)) {
        // Pages acquired for a serviced fault are KEPT pinned so the restart on the next
        // tick can actually use them. Any other failure holds nothing.
        if (!holdingPages) releasePages();
        stalledOnMemory = !isFinished;   // a violation ends the process, it is not a stall
        return;
    }

    executeOne(ins);
    releasePages();
    stalledOnMemory = false;

    currentLine++;
    if (currentLine >= static_cast<int>(instructions.size())) isFinished = true;
}

// ----- Paging -----
bool Process::inRange(std::size_t addr) const {
    // a uint16 occupies 2 bytes, so addr+1 must also be inside our address space
    return memoryBytes >= 2 && addr + 1 < memoryBytes;
}


void Process::raiseViolation(std::size_t addr) {
    std::ostringstream hex;
    hex << "0x" << std::hex << std::uppercase << addr;

    violated         = true;
    violationAddress = hex.str();
    violationTime    = clockTime();
    isFinished       = true;

    std::lock_guard<std::mutex> lock(logMutex);
    printLogs.push_back("(" + timestamp() + ") Core:" + std::to_string(coreId)
                        + " ACCESS VIOLATION at " + violationAddress);
}

bool Process::ensurePages(const Instruction& ins) {
    if (!mm) return true;

    // Does this instruction touch a variable? Then the symbol table segment must be in.
    bool needsSymbolTable = false;
    switch (ins.type) {
    case InstrType::DECLARE:
    case InstrType::ADD:
    case InstrType::SUBTRACT:
    case InstrType::READ:
        needsSymbolTable = true;
        break;
    case InstrType::WRITE:
        needsSymbolTable = !isNumericToken(ins.a2);        // writing a variable's value
        break;
    case InstrType::PRINT:
        needsSymbolTable = !ins.printVar.empty();
        break;
    default:
        break;
    }

    bool touchesMemory = (ins.type == InstrType::READ || ins.type == InstrType::WRITE);

    if (touchesMemory && !inRange(ins.address)) {          // outside our memory space
        raiseViolation(ins.address);
        return false;
    }

    // Every byte this instruction reaches for. A uint16 spans two bytes and may straddle
    // a page boundary, so both ends are listed.
    std::vector<std::size_t> addrs;
    if (needsSymbolTable) { addrs.push_back(0); addrs.push_back(SYMBOL_TABLE_BYTES - 1); }
    if (touchesMemory)    { addrs.push_back(ins.address); addrs.push_back(ins.address + 1); }
    if (addrs.empty()) return true;                    // touches no memory at all

    // One instruction can need the symbol table page and a data page resident at the
    // same moment. If physical memory has fewer frames than that, no amount of paging
    // will ever satisfy it and the process would retry forever, taking a core with it.
    // Shut it down instead: an honest error beats a hang in a graded run.
    {
        std::size_t frameSize = mm->frameSize();
        std::vector<std::size_t> pages;
        for (std::size_t a : addrs) {
            std::size_t vp = a / frameSize;
            if (std::find(pages.begin(), pages.end(), vp) == pages.end()) pages.push_back(vp);
        }

        if (pages.size() > mm->totalFrames()) {
            raiseViolation(ins.address);
            std::lock_guard<std::mutex> lock(logMutex);
            printLogs.push_back("(" + timestamp() + ") Core:" + std::to_string(coreId)
                                + " needs " + std::to_string(pages.size())
                                + " resident pages but physical memory has only "
                                + std::to_string(mm->totalFrames())
                                + " frame(s) - raise max-overall-mem or mem-per-frame");
            return false;
        }
    }

    // All of them or none of them, in one step. Taking the pages one at a time is what
    // livelocked two processes sharing two frames.
    MemoryManager::Acquire r = mm->acquirePages(pid, addrs);
    if (r == MemoryManager::Acquire::FAILED) {
        holdingPages = false;                         // memory is fully pinned right now
        return false;
    }
    if (r == MemoryManager::Acquire::FAULTS_SERVICED) {
        ++pageFaults;
        holdingPages = true;                          // keep them through the restart
        return false;                                 // restart the instruction next tick
    }
    return true;
}

void Process::releasePages() {
    if (mm) mm->unpinProcess(pid);
    holdingPages = false;
}

// ----- Instruction dispatch -----
void Process::executeOne(const Instruction& ins) {
    switch (ins.type) {
    case InstrType::DECLARE:
        setVariable(ins.a1, clampU16(ins.value));
        break;

    case InstrType::ADD: {
        long r = static_cast<long>(resolve(ins.a2)) + static_cast<long>(resolve(ins.a3));
        setVariable(ins.a1, clampU16(r));                      // overflow clamps to 65535
        break;
    }

    case InstrType::SUBTRACT: {
        long r = static_cast<long>(resolve(ins.a2)) - static_cast<long>(resolve(ins.a3));
        setVariable(ins.a1, clampU16(r));                      // underflow clamps to 0
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
        // FOR is unrolled during generation/parsing; nothing to execute here.
        break;

    case InstrType::READ: {
        uint16_t v = mm ? mm->readU16(pid, ins.address) : 0;   // uninitialised reads as 0
        setVariable(ins.a1, v);
        break;
    }

    case InstrType::WRITE: {
        uint16_t v = resolve(ins.a2);
        if (mm) mm->writeU16(pid, ins.address, v);
        break;
    }
    }
}

// ----- Variables live in the symbol table segment, not in a std::map -----
int Process::slotFor(const std::string& varName, bool createIfMissing) {
    auto it = varSlot.find(varName);
    if (it != varSlot.end()) return it->second;
    if (!createIfMissing) return -1;

    // Symbol table is full: succeeding declarations are ignored (spec).
    if (nextSlot >= MAX_VARIABLES) return -1;

    int slot = nextSlot++;
    varSlot[varName] = slot;
    if (mm) mm->writeU16(pid, static_cast<std::size_t>(slot) * 2, 0);
    return slot;
}

void Process::setVariable(const std::string& varName, uint16_t value) {
    int slot = slotFor(varName, true);
    if (slot < 0) return;                                      // table full -> ignored
    if (mm) mm->writeU16(pid, static_cast<std::size_t>(slot) * 2, value);
}

// A token is either a numeric literal or a variable name. Variables that are
// referenced before declaration are auto-declared with value 0 (per spec).
uint16_t Process::resolve(const std::string& token) {
    if (isNumericToken(token)) {
        try { return clampU16(std::stol(token)); } catch (...) { return 0; }
    }
    int slot = slotFor(token, true);
    if (slot < 0) return 0;                                    // table full
    return mm ? mm->readU16(pid, static_cast<std::size_t>(slot) * 2) : 0;
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

std::string Process::clockTime() {
    std::time_t now = std::time(nullptr);
    std::tm tm_info{};
#ifdef _WIN32
    localtime_s(&tm_info, &now);
#else
    localtime_r(&now, &tm_info);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_info);
    return std::string(buf);
}

// ----- User-supplied programs (screen -c) -----
bool Process::parseProgram(const std::string& text,
                           std::vector<Instruction>& out,
                           std::string& error) {
    std::string body = trim(text);
    if (body.size() >= 2 && body.front() == '"' && body.back() == '"')
        body = body.substr(1, body.size() - 2);

    out.clear();
    for (const std::string& piece : splitTop(body, ';'))
        if (!parseOne(piece, out, error, 1)) return false;

    if (out.empty()) { error = "no instructions given"; return false; }
    return true;
}

int Process::countTopLevel(const std::string& text) {
    std::string body = trim(text);
    if (body.size() >= 2 && body.front() == '"' && body.back() == '"')
        body = body.substr(1, body.size() - 2);

    int n = 0;
    for (const std::string& piece : splitTop(body, ';'))
        if (!trim(piece).empty()) ++n;
    return n;
}

void Process::setProgram(const std::vector<Instruction>& program) {
    instructions = program;
    totalLines   = static_cast<int>(instructions.size());
    currentLine  = 0;
    isFinished   = (totalLines == 0);
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

    // MCO2: READ/WRITE join the mix, but only if the process actually has data space
    // beyond its 64-byte symbol table - otherwise every generated process would die
    // of an access violation instead of demonstrating the scheduler.
    const bool allowMemoryOps = memoryBytes > SYMBOL_TABLE_BYTES + 2;
    std::size_t maxAddr = allowMemoryOps ? (memoryBytes - 2) : 0;

    std::uniform_int_distribution<int> pick(0, allowFor ? 5 : 4);
    std::uniform_int_distribution<int> memPick(0, 1);
    std::uniform_int_distribution<int> vsel(0, 4);
    std::uniform_int_distribution<int> val(0, 500);
    std::uniform_int_distribution<int> slp(1, 3);
    std::uniform_int_distribution<int> rep(2, 3);
    std::uniform_int_distribution<int> memChance(0, 3);         // ~25% memory instructions
    std::uniform_int_distribution<std::size_t> addrSel(
        SYMBOL_TABLE_BYTES / 2, allowMemoryOps ? maxAddr / 2 : SYMBOL_TABLE_BYTES / 2);

    while (static_cast<int>(instructions.size()) < target) {

        if (allowMemoryOps && memChance(rng) == 0) {
            Instruction ins;
            ins.address = addrSel(rng) * 2;                     // 2-byte aligned, in range
            if (memPick(rng) == 0) {
                ins.type = InstrType::WRITE;
                ins.a2   = std::to_string(val(rng));
            } else {
                ins.type = InstrType::READ;
                ins.a1   = vars[vsel(rng)];
            }
            instructions.push_back(ins);
            continue;
        }

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
