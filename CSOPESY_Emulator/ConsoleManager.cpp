#include "ConsoleManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <vector>

namespace {
    std::string trimStr(const std::string& s) {
        std::size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) return "";
        std::size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    bool isPowerOfTwo(std::size_t v) {
        return v != 0 && (v & (v - 1)) == 0;
    }

    // config.txt memory parameters must all be powers of 2 in [2^6, 2^16].
    bool validMemParam(std::size_t v) {
        return isPowerOfTwo(v) && v >= 64 && v <= 65536;
    }

    // Where "initialize" looks for config.txt. The working directory depends on how the
    // program was launched - CLion and Visual Studio run the exe from their build folder,
    // the command line usually runs it from the project root - so try the layouts that
    // actually occur instead of failing with "could not open config.txt".
    const char* CONFIG_CANDIDATES[] = {
        "config.txt",                          // cwd (exe folder: CMake copies it there)
        "CSOPESY_Emulator/config.txt",         // cwd = project root
        "../CSOPESY_Emulator/config.txt",      // cwd = cmake-build-debug/
        "../config.txt",
        "../../CSOPESY_Emulator/config.txt",   // cwd = build/Debug/
        "../../config.txt",
    };
}

// ----- CONSTRUCTOR -----
ConsoleManager::ConsoleManager() {
	scheduler = std::make_shared<Scheduler>();
}

//
// ----- MAIN LOOP -----
//

void ConsoleManager::run() {
	displayHeader();

	std::string	input;
	while (running) {
		if (currentState == State::MAIN_MENU) {
			std::cout << "\nroot:\\>";
		}
		else {
			// show which process screen we're in
			std::cout << "\n[" << attachedProcess->name << "]:\\>";
		}

		if (!std::getline(std::cin, input)) break;

		if (currentState == State::MAIN_MENU)
			handleMainMenuCommand(trimStr(input));
		else
			handleProcessScreenCommand(trimStr(input));
	}
}

//
// ----- DISPLAY HEADER BANNER ----
//

void ConsoleManager::displayHeader() {
	std::cout << " _______  _______  _______  _______  _______  _______  __   __ " << std::endl;
	std::cout << "|       ||       ||       ||       ||       ||       ||  | |  |" << std::endl;
	std::cout << "|       ||  _____||   _   ||    _  ||    ___||  _____||  |_|  |" << std::endl;
	std::cout << "|       || |_____ |  | |  ||   |_| ||   |___ | |_____ |       |" << std::endl;
	std::cout << "|      _||_____  ||  |_|  ||    ___||    ___||_____  ||_     _|" << std::endl;
	std::cout << "|     |_  _____| ||       ||   |    |   |___  _____| |  |   |  " << std::endl;
	std::cout << "|_______||_______||_______||___|    |_______||_______|  |___|  " << std::endl;
	std::cout << "------------------------------------------------------------" << std::endl;
	std::cout << "Welcome to CSOPESY Emulator! (MCO2 - Multitasking OS)" << std::endl;
	std::cout << "Developers: Alviar, Kelvin | Calpoporo, Angelo | Carlos, Miguel | Tujan, Nio" << std::endl;
	std::cout << "Last updated: 08-01-2026" << std::endl;
	std::cout << "------------------------------------------------------------" << std::endl;
}

// Clear the console screen (platform-dependent)
void ConsoleManager::clearScreen() {
#ifdef _WIN32
	system("cls");
#else
	system("clear");
#endif
}

//
// Load and Validate Config from config.txt, then initialize the Scheduler
//

void ConsoleManager::loadConfig() {
	initialized = false;

	std::ifstream file;
	std::string   configPath;
	for (const char* candidate : CONFIG_CANDIDATES) {
		file.open(candidate);
		if (file.is_open()) { configPath = candidate; break; }
		file.clear();
	}
	if (!file.is_open()) {
		std::cout << "Error: Could not open config.txt. Looked in:" << std::endl;
		for (const char* candidate : CONFIG_CANDIDATES)
			std::cout << "  " << candidate << std::endl;
		return;
	}

	std::string key;
	while (file >> key) {
		if (key == "num-cpu") file >> config.num_cpu;
		else if (key == "scheduler") {
			file >> config.scheduler;
			// remove surrounding quotes if present e.g ., "rr" -> rr
			if (config.scheduler.size() >= 2 && config.scheduler.front() == '"')
				config.scheduler = config.scheduler.substr(1, config.scheduler.size() - 2);
		}
		else if (key == "quantum-cycles")      file >> config.quantum_cycles;
		else if (key == "batch-process-freq")  file >> config.batch_process_freq;
		else if (key == "min-ins")             file >> config.min_ins;
		else if (key == "max-ins")             file >> config.max_ins;
		else if (key == "delay-per-exec")      file >> config.delay_per_exec;
		// ----- MCO2 memory parameters -----
		else if (key == "max-overall-mem")     file >> config.max_overall_mem;
		else if (key == "mem-per-frame")       file >> config.mem_per_frame;
		else if (key == "min-mem-per-proc")    file >> config.min_mem_per_proc;
		else if (key == "max-mem-per-proc")    file >> config.max_mem_per_proc;
	}

	file.close();

	// Validate ranges
	if (config.num_cpu < 1 || config.num_cpu > 128) {
		std::cout << "Error: num-cpu must be between 1 and 128." << std::endl;
		return;
	}
	if (config.scheduler != "rr" && config.scheduler != "fcfs") {
		std::cout << "Error: scheduler must be \"rr\" or \"fcfs\"." << std::endl;
		return;
	}
	if (config.min_ins > config.max_ins) {
		std::cout << "Error: min-ins cannot be greater than max-ins." << std::endl;
		return;
	}

	// MCO2: all four memory parameters are powers of 2 within [2^6, 2^16]
	struct { const char* name; std::size_t value; } memParams[] = {
		{ "max-overall-mem",  config.max_overall_mem  },
		{ "mem-per-frame",    config.mem_per_frame    },
		{ "min-mem-per-proc", config.min_mem_per_proc },
		{ "max-mem-per-proc", config.max_mem_per_proc },
	};
	for (const auto& p : memParams) {
		if (!validMemParam(p.value)) {
			std::cout << "Error: " << p.name
			          << " must be a power of 2 between 64 and 65536 bytes." << std::endl;
			return;
		}
	}
	if (config.mem_per_frame > config.max_overall_mem) {
		std::cout << "Error: mem-per-frame cannot exceed max-overall-mem." << std::endl;
		return;
	}
	if (config.min_mem_per_proc > config.max_mem_per_proc) {
		std::cout << "Error: min-mem-per-proc cannot be greater than max-mem-per-proc." << std::endl;
		return;
	}

	initialized = true;
	scheduler->initialize(config);

	std::size_t frames = config.max_overall_mem / config.mem_per_frame;
	std::cout << "Initialized successfully. (config: " << configPath << ")" << std::endl;
	std::cout << "  Scheduler : " << config.scheduler
	          << " on " << config.num_cpu << " core(s)" << std::endl;
	std::cout << "  Memory    : " << config.max_overall_mem << " bytes / "
	          << config.mem_per_frame << " bytes per frame = "
	          << frames << " frame(s)" << std::endl;

	// An instruction that reads a variable AND touches a data address needs two pages
	// resident at once. With a single frame that can never happen, so such a process is
	// shut down rather than left spinning. The config is legal, so this is a warning.
	if (frames < 2) {
		std::cout << "  Warning   : only 1 frame exists. Processes that use READ/WRITE "
		             "outside their symbol table cannot run under this configuration."
		          << std::endl;
	}
}


//
// Main Menu Command Handlers
//

void ConsoleManager::handleMainMenuCommand(const std::string& input) {
	if (input.empty()) return;

	if (input == "exit") {
		// Leave a current backing-store file behind: the spec says it must be readable
		// at any time, and a run shorter than the flush throttle would otherwise end
		// with a file whose counters disagree with the last vmstat.
		if (initialized && scheduler) scheduler->memory().flushBackingStore(true);
		std::cout << "Exiting CSOPESY Emulator. Goodbye!" << std::endl;
		running = false;
		return;
	}

	if (input == "initialize") {
		loadConfig();
		return;
	}

	if (input == "cls" || input == "clear") {
		clearScreen();
		displayHeader();
		return;
	}

	// All commnads below require initialization
	if (!initialized) {
		std::cout << "Error: Please run 'initialize' first." << std::endl;
		return;
	}

	if (input.rfind("screen", 0) == 0) {
		//Everythinh after "screen" (trim leading space)
		std::string args = (input.size() > 6) ? trimStr(input.substr(6)) : "";
		handleScreenCommand(args);
	}
	// "scheduler-test" is the name earlier handouts and test cases use for the same
	// command. Accepting both costs nothing and a rejected command costs a test case.
	else if (input == "scheduler-start" || input == "scheduler-test")
	{
		scheduler->startGeneration();
		std::cout << "Scheduler started. Generating processes..." << std::endl;
	}
	else if (input == "scheduler-stop")
	{
		scheduler->stopGeneration();
		std::cout << "Scheduler stopped." << std::endl;
	}
	else if (input == "report-util")
	{
		handleReportUtil();
	}
	else if (input == "process-smi")
	{
		displaySystemSMI();
	}
	else if (input == "vmstat")
	{
		displayVmstat();
	}
	else {
		std::cout << "Unknown Command: '" << input << "'" << std::endl;
	}
}

//
// Handle "screen" subcommands: -s <name> <mem>, -c <name> <mem> "<instrs>", -r <name>, -ls
//

void ConsoleManager::handleScreenCommand(const std::string& args) {
	static const char* usage =
		"Usage: screen -s <name> <memory> | screen -c <name> <memory> \"<instructions>\""
		" | screen -r <name> | screen -ls";

	if (args.empty()) {
		std::cout << usage << std::endl;
		return;
	}

	if (args == "-ls") {
		handleScreenList();
		return;
	}

	if (args.size() < 3) {
		std::cout << usage << std::endl;
		return;
	}

	std::string flag = args.substr(0, 2);            // "-s" / "-c" / "-r"
	std::string rest = trimStr(args.substr(2));

	if (flag == "-s")      handleScreenCreate(rest);
	else if (flag == "-c") handleScreenCustom(rest);
	else if (flag == "-r") handleScreenResume(rest);
	else std::cout << "Unknown screen flag: '" << flag << "'" << std::endl;
}

// All process memory sizes are powers of 2 within [2^6, 2^16] bytes.
bool ConsoleManager::validMemorySize(std::size_t bytes) const {
	return isPowerOfTwo(bytes) && bytes >= 64 && bytes <= 65536;
}

// screen -s <name> [<memory>]
// The memory size is optional: test cases are written both ways, and refusing the
// short form would fail a case over syntax rather than over behavior. Without it the
// process gets max-mem-per-proc, the largest size a generated process may be rolled.
void ConsoleManager::handleScreenCreate(const std::string& rest) {
	std::istringstream iss(rest);
	std::string name, memToken;
	iss >> name >> memToken;

	if (name.empty()) {
		std::cout << "Usage: screen -s <name> [<memory>]" << std::endl;
		return;
	}

	std::size_t memBytes = 0;
	if (memToken.empty()) {
		memBytes = config.max_mem_per_proc;
		std::cout << "No memory size given; using max-mem-per-proc = "
		          << memBytes << " bytes." << std::endl;
	} else {
		try { memBytes = static_cast<std::size_t>(std::stoull(memToken)); }
		catch (...) { std::cout << "invalid memory allocation" << std::endl; return; }
	}

	if (!validMemorySize(memBytes)) {
		std::cout << "invalid memory allocation" << std::endl;
		return;
	}

	auto process = scheduler->createProcess(name, memBytes);
	if (!process) {
		std::cout << "Error: Could not create process '" << name << "'." << std::endl;
		return;
	}

	attachedProcess = process;
	currentState = State::PROCESS_SCREEN;
	clearScreen();
	displayProcessSMI();
}

// screen -c <name> [<memory>] "<instruction; instruction; ...>"
// The memory size is optional here too. The token after the name is treated as a size
// only when it is entirely numeric; otherwise it is the first word of the program and
// the size is derived from the addresses the program actually uses.
void ConsoleManager::handleScreenCustom(const std::string& rest) {
	std::istringstream iss(rest);
	std::string name;
	iss >> name;

	if (name.empty()) {
		std::cout << "Usage: screen -c <name> [<memory>] \"<instructions>\"" << std::endl;
		return;
	}

	std::size_t memBytes    = 0;
	bool        explicitMem = false;

	std::streampos afterName = iss.tellg();
	std::string    memToken;
	iss >> memToken;

	if (!memToken.empty() && memToken.find_first_not_of("0123456789") == std::string::npos) {
		try { memBytes = static_cast<std::size_t>(std::stoull(memToken)); explicitMem = true; }
		catch (...) { std::cout << "invalid memory allocation" << std::endl; return; }
	} else {
		iss.clear();
		iss.seekg(afterName);            // not a size: hand the token back to the program
	}

	if (explicitMem && !validMemorySize(memBytes)) {
		std::cout << "invalid memory allocation" << std::endl;
		return;
	}

	// everything after the name (and the optional size) is the instruction string
	std::string program;
	std::getline(iss, program);
	program = trimStr(program);
	if (program.empty()) {
		std::cout << "invalid command" << std::endl;
		return;
	}

	// 1 - 50 instructions, counted before FOR loops are unrolled
	int count = Process::countTopLevel(program);
	if (count < 1 || count > 50) {
		std::cout << "invalid command" << std::endl;
		return;
	}

	std::vector<Process::Instruction> parsed;
	std::string error;
	if (!Process::parseProgram(program, parsed, error)) {
		std::cout << "invalid command (" << error << ")" << std::endl;
		return;
	}

	// No size given: use the same fallback as screen -s. Deriving a size from the
	// addresses the program happens to name would be friendlier, but it would also
	// silently resize a process out of the access violation the spec requires - and
	// the grader can set max-mem-per-proc in config.txt, which is the supported knob.
	if (!explicitMem) {
		memBytes = config.max_mem_per_proc;
		std::cout << "No memory size given; using max-mem-per-proc = "
		          << memBytes << " bytes." << std::endl;
		if (!validMemorySize(memBytes)) {
			std::cout << "invalid memory allocation" << std::endl;
			return;
		}
	}

	auto process = scheduler->createProcess(name, memBytes, parsed);
	if (!process) {
		std::cout << "Error: Could not create process '" << name << "'." << std::endl;
		return;
	}

	attachedProcess = process;
	currentState = State::PROCESS_SCREEN;
	clearScreen();
	displayProcessSMI();
}

// screen -r <name>
void ConsoleManager::handleScreenResume(const std::string& name) {
	if (name.empty()) {
		std::cout << "Error: Process name cannot be empty." << std::endl;
		return;
	}

	auto any = scheduler->lookupProcess(name);

	// MCO2: a process killed by an access violation reports how and when it died.
	if (any && any->violated) {
		std::cout << "Process " << name
		          << " shut down due to memory access violation error that occurred at "
		          << any->violationTime << ". " << any->violationAddress << " invalid."
		          << std::endl;
		return;
	}

	auto process = scheduler->findProcess(name);
	if (!process) {
		std::cout << "Process " << name << " not found." << std::endl;
		return;
	}

	attachedProcess = process;
	currentState = State::PROCESS_SCREEN;
	clearScreen();
	displayProcessSMI();
}

//
// screen -ls: List all processes (running + finished) in a table
//
void ConsoleManager::handleScreenList() {
	std::cout << std::endl;
	printProcessTable(std::cout, CONSOLE_LIST_ROWS);
}

//
// report-util: print to terminal AND wite to csopesy-log.txt the process table and CPU utilization
//
void ConsoleManager::handleReportUtil() {
	handleScreenList();

	std::ofstream file("csopesy-log.txt");
	if (!file.is_open()) {
		std::cout << "Error: Could not open csopesy-log.txt for writing." << std::endl;
		return;
	}
	printProcessTable(file);          // no row cap: the log file gets every process
	file.close();

	std::cout << "Report generated at csopesy-log.txt" << std::endl;
}

// Shared: format and print the full process table to any stream
// (used by both handleScreenList and handleReportUtil)
//
void ConsoleManager::printProcessTable(std::ostream& out, std::size_t maxRows) {
	double utilPct = scheduler->getCpuUtilization();
	int coresUsed  = scheduler->getCoresUsed();
	int coresAvail = scheduler->getCoresAvailable();

	// '\n' rather than std::endl throughout: std::endl flushes, and one flush per row
	// is what makes a Windows console lock up when a stress config has produced
	// thousands of processes. The single flush at the end is enough.
	std::ostringstream buf;
	buf << "CPU Utilization: " << std::fixed << std::setprecision(0) << utilPct << "%\n";
	buf << "Cores Used: " << coresUsed << '\n';
	buf << "Cores Available: " << coresAvail << '\n';
	buf << '\n';
	buf << std::string(78, '-') << '\n';

	auto processes = scheduler->getAllProcesses();

	// Print at most maxRows rows per section (0 = unlimited, used for the log file).
	// The hidden ones are still counted, so no information is lost - a 20,000-process
	// stress run stays readable on camera instead of scrolling for a minute.
	auto section = [&](const char* title, bool finished) {
		buf << title << '\n';
		std::size_t shown = 0, hidden = 0;
		for (const auto& p : processes) {
			if (p->isFinished != finished) continue;
			if (maxRows > 0 && shown >= maxRows) { hidden++; continue; }

			std::string label = finished ? (p->violated ? "Violation" : "Finished")
			                             : ("Core " + std::to_string(p->coreId) + ":");
			buf << std::left  << std::setw(16) << p->name
			    << std::left  << std::setw(28) << ("(" + p->createdAt + ")")
			    << std::left  << std::setw(10) << label
			    << std::right << std::setw(6)  << p->currentLine
			    << " / " << p->totalLines
			    << '\n';
			shown++;
		}
		if (hidden > 0)
			buf << "... and " << hidden << " more (showing the first " << maxRows
			    << " - report-util writes the full list)\n";
	};

	section("Running Processes:", false);
	buf << '\n';
	section("Finished Processes:", true);

	buf << std::string(78, '-') << '\n';

	out << buf.str() << std::flush;
}

//
// MCO2 process-smi (main menu): nvidia-smi style memory summary
//
void ConsoleManager::displaySystemSMI() {
	MemoryManager& mm = scheduler->memory();
	mm.flushBackingStore(true);          // make the backing store file current

	std::size_t total = mm.totalMemory();
	std::size_t used  = mm.usedMemory();
	double memUtil    = (total == 0) ? 0.0 : (used * 100.0) / static_cast<double>(total);

	std::cout << std::endl;
	std::cout << "--------------------------------------------------" << std::endl;
	std::cout << "| PROCESS-SMI V01.00   Driver Version: 01.00      |" << std::endl;
	std::cout << "--------------------------------------------------" << std::endl;
	std::cout << "CPU-Util: " << std::fixed << std::setprecision(0)
	          << scheduler->getCpuUtilization() << "%" << std::endl;
	std::cout << "Memory Usage: " << used << "B / " << total << "B" << std::endl;
	std::cout << "Memory Util: " << std::fixed << std::setprecision(0) << memUtil << "%" << std::endl;
	std::cout << std::endl;
	std::cout << "==================================================" << std::endl;
	std::cout << "Running processes and memory usage:" << std::endl;
	std::cout << "--------------------------------------------------" << std::endl;

	auto processes = scheduler->getAllProcesses();
	std::ostringstream buf;                  // buffered for the same reason as screen -ls
	std::size_t shown = 0, hidden = 0;
	for (const auto& p : processes) {
		if (p->isFinished) continue;
		if (shown >= CONSOLE_LIST_ROWS) { hidden++; continue; }
		buf << std::left  << std::setw(20) << p->name
		    << std::right << std::setw(8)  << mm.processResidentBytes(p->pid) << "B"
		    << "   (allocated " << p->memoryBytes << "B)\n";
		shown++;
	}
	if (shown == 0)  buf << "(no running processes)\n";
	if (hidden > 0)  buf << "... and " << hidden << " more running (showing the first "
	                     << CONSOLE_LIST_ROWS << ")\n";

	buf << "--------------------------------------------------\n";
	std::cout << buf.str() << std::flush;
}

//
// MCO2 vmstat: the fine-grained view
//
void ConsoleManager::displayVmstat() {
	MemoryManager& mm = scheduler->memory();
	mm.flushBackingStore(true);

	auto processes = scheduler->getAllProcesses();
	std::size_t active = 0, inactive = 0;
	for (const auto& p : processes) {
		if (p->isFinished) inactive++;
		else               active++;
	}

	std::cout << std::endl;
	std::cout << std::setw(14) << mm.totalMemory()          << " bytes total memory"     << std::endl;
	std::cout << std::setw(14) << mm.usedMemory()           << " bytes used memory"      << std::endl;
	std::cout << std::setw(14) << mm.freeMemory()           << " bytes free memory"      << std::endl;
	std::cout << std::setw(14) << active                    << " active processes"       << std::endl;
	std::cout << std::setw(14) << inactive                  << " inactive processes"     << std::endl;
	std::cout << std::setw(14) << scheduler->getIdleTicks()   << " idle cpu ticks"       << std::endl;
	std::cout << std::setw(14) << scheduler->getActiveTicks() << " active cpu ticks"     << std::endl;
	std::cout << std::setw(14) << scheduler->getTotalTicks()  << " total cpu ticks"      << std::endl;
	std::cout << std::setw(14) << mm.pagedIn()              << " num paged in"           << std::endl;
	std::cout << std::setw(14) << mm.pagedOut()             << " num paged out"          << std::endl;
}

//
// Process screen: dispatch commands
//
void ConsoleManager::handleProcessScreenCommand(const std::string& input) {
	if (input.empty()) return;

	if (input == "exit") {
		// Return to main menu WITHOUT terminating the process
		attachedProcess = nullptr;
		currentState = State::MAIN_MENU;
		clearScreen();
		displayHeader();
		return;
	}
	if (input == "process-smi") {
		displayProcessSMI();
		return;
	}

	// Anything else is handled as if it had been typed at the main menu. A test case
	// that says "create two processes, then type screen -ls" would otherwise force an
	// undocumented "exit" in between, and typing a real command should never be an
	// error just because a process screen happens to be open.
	handleMainMenuCommand(input);
}

//
// Display Procss info + logs (process-smi inside a process screen)
//
void ConsoleManager::displayProcessSMI() {
	if (!attachedProcess) return;

	std::cout << std::endl;
	std::cout << "Process Name: " << attachedProcess->name << std::endl;
	std::cout << "ID:           " << attachedProcess->pid << std::endl;
	std::cout << "Created:      " << attachedProcess->createdAt << std::endl;
	std::cout << "Memory:       " << attachedProcess->memoryBytes << "B allocated, "
	          << scheduler->memory().processResidentBytes(attachedProcess->pid)
	          << "B resident" << std::endl;
	std::cout << "Page faults:  " << attachedProcess->pageFaults << std::endl;
	std::cout << std::endl;
	std::cout << "Current instruction line: " << attachedProcess->currentLine << std::endl;
	std::cout << "Lines of code:            " << attachedProcess->totalLines << std::endl;
	std::cout << std::endl;

	// Safely read print logs while the scheduler might be writing to them
	{
		std::lock_guard<std::mutex> lock(attachedProcess->logMutex);
		if (!attachedProcess->printLogs.empty()) {
			std::cout << "Logs:" << std::endl;
			for (const auto& entry : attachedProcess->printLogs)
				std::cout << entry << std::endl;
			std::cout << std::endl;
		}
	}

	if (attachedProcess->violated) {
		std::cout << "Process " << attachedProcess->name
		          << " shut down due to memory access violation error that occurred at "
		          << attachedProcess->violationTime << ". "
		          << attachedProcess->violationAddress << " invalid." << std::endl;
	}
	else if (attachedProcess->isFinished) {
		std::cout << "Finished!" << std::endl;
	}
}

//
// Get the current time as a formatted string
//
std::string ConsoleManager::getCurrentTimestamp() {
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
