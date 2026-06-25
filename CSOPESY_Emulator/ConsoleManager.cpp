#include "ConsoleManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

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

		std::getline(std::cin, input);

		if (currentState == State::MAIN_MENU)
			handleMainMenuCommand(input);
		else
			handleProcessScreenCommand(input);
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
	std::cout << "Welcome to CSOPESY Emulator!" << std::endl;
	std::cout << "Developers: Alviar, Kelvin | Calpoporo, Angelo | Carlos, Miguel | Tujan, Nio" << std::endl;
	std::cout << "Last updated: 06-25-2026" << std::endl;
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

	std::ifstream file("config.txt");
	if (!file.is_open()) {
		std::cout << "Error: Could not open config.txt" << std::endl;
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

	initialized = true;
	scheduler->initialize(config);
	std::cout << "Initialized successfully." << std::endl;
}


//
// Main Menu Command Handlers
//

void ConsoleManager::handleMainMenuCommand(const std::string& input) {
	if (input.empty()) return;

	if (input == "exit") {
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
		std::string args = (input.size() > 6) ? input.substr(7) : "";
		handleScreenCommand(args);	
	}
	else if (input == "scheduler-start")
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
	else {
		std::cout << "Unknown Command: '" << input << "'" << std::endl;
	}
}

//
// Handle "screen" subcommands: -s <name>, -r <name>, -ls
//

void ConsoleManager::handleScreenCommand(const std::string& args) {
	if (args.empty()) {
		std::cout << "Usage: screen -s <name> | screen -r <name> | screen -ls" << std::endl;
		return;
	}

	// --- screen -ls ---
	if (args == "-ls") {
		handleScreenList();
		return;
	}

	// --- screen -s or -r: need at least "-s x" (4 chars) ---
	if (args.size() < 4) {
		std::cout << "Usage: screen -s <name> | screen -r <name> | screen -ls" << std::endl;
		return;
	}

	std::string flag = args.substr(0, 2);          // "-s" or "-r"
	std::string processName = args.substr(3);       // everything after "-s " or "-r "

	if (flag != "-s" && flag != "-r") {
		std::cout << "Unknown screen flag: '" << flag << "'" << std::endl;
		return;
	}

	if (processName.empty()) {
		std::cout << "Error: Process name cannot be empty." << std::endl;
		return;
	}

	if (flag == "-s") {
		// Create a new process and attach to it
		auto process = scheduler->createProcess(processName);
		if (!process) {
			std::cout << "Error: Could not create process '" << processName << "'." << std::endl;
			return;
		}
		attachedProcess = process;
		currentState = State::PROCESS_SCREEN;
		clearScreen();
		displayProcessSMI();
	}
	else { // -r
		// Re-attach to an existing process
		auto process = scheduler->findProcess(processName);
		if (!process) {
			std::cout << "Process " << processName << " not found." << std::endl;
			return;
		}
		attachedProcess = process;
		currentState = State::PROCESS_SCREEN;
		clearScreen();
		displayProcessSMI();
	
	}
}

// 
// screen -ls: List all processes (running + finished) in a table
//
void ConsoleManager::handleScreenList() {
	std::cout << std::endl;
	printProcessTable(std::cout);
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
	printProcessTable(file);
	file.close();

	std::cout << "Report generated at csopesy-log.txt" << std::endl;
}

// Shared: format and print the full process table to any stream 
// (used by both handleScreenList and handleReportUtil)
//
void ConsoleManager::printProcessTable(std::ostream& out) {
	double utilPct = scheduler->getCpuUtilization();
	int coresUsed  = scheduler->getCoresUsed();
	int coresAvail = scheduler->getCoresAvailable();

	out << "CPU Utilization: " << std::fixed << std::setprecision(0) << utilPct << "%" << std::endl;
	out << "Cores Used: " << coresUsed << std::endl;
	out << "Cores Available: " << coresAvail << std::endl;
	out << std::endl;
	out << std::string(72, '-') << std::endl;

	auto processes = scheduler->getAllProcesses();

	out << "Running Processes:" << std::endl;
	for (const auto& p : processes) {
		if (!p->isFinished) {
			std::string coreLabel = "Core " + std::to_string(p->coreId) + ":";
			out << std::left << std::setw(16) << p->name
				<< std::left << std::setw(28) << ("(" + p->createdAt + ")")
				<< std::left << std::setw(10) << coreLabel
				<< std::right << std::setw(6) << p->currentLine
				<< " / " << p->totalLines
				<< std::endl;
		}
	}

	out << std::endl;
	out << "Finished Processes:" << std::endl;
	for (const auto& p : processes) {
		if (p->isFinished) {
			out << std::left << std::setw(16) << p->name
				<< std::left << std::setw(28) << ("(" + p->createdAt + ")")
				<< std::left << std::setw(10) << "Finished"
				<< std::right << std::setw(6) << p->totalLines
				<< " / " << p->totalLines
				<< std::endl;
		}
	}

	out << std::string(72, '-') << std::endl;
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

	std::cout << "Unknown command: '" << input << "'. Available: process-smi, exit" << std::endl;
}

//
// Display Procss info + logs (process-smi)
//
void ConsoleManager::displayProcessSMI() {
	if (!attachedProcess) return;

	std::cout << std::endl;
	std::cout << "Process Name: " << attachedProcess->name << std::endl;
	std::cout << "ID:           " << attachedProcess->pid << std::endl;
	std::cout << "Created:      " << attachedProcess->createdAt << std::endl;
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

	if (attachedProcess->isFinished)
		std::cout << "Finished!" << std::endl;
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


