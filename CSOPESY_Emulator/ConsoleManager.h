#pragma once

#include <string>
#include <memory>
#include <ostream>
#include <vector>
#include "Config.h"
#include "Process.h"
#include "Scheduler.h"

class ConsoleManager {
public:
	ConsoleManager();
	void run();

private:
	// State Machine
	enum class State {
		MAIN_MENU, PROCESS_SCREEN
	};
	State currentState = State::MAIN_MENU;

	// Config
	Config config;
	bool initialized = false;

	// Scheduler
	std::shared_ptr<Scheduler> scheduler;

	// Currently attached process (only valid in PROCESS_SCREEN)
	std::shared_ptr<Process> attachedProcess;

	// Flag to exit the run loop cleanly
	bool running = true;

	// Display Helpers
	void displayHeader();
	void clearScreen();

	// Config Loading
	void loadConfig();

	// Main Menu Handlers
	void handleMainMenuCommand(const std::string& input);
	void handleScreenCommand(const std::string& args);
	void handleScreenList();
	void handleReportUtil();

	// MCO2 main-menu views
	void displaySystemSMI();     // process-smi: memory summary + per-process usage
	void displayVmstat();        // vmstat: fine-grained memory and tick counters

	// MCO2 screen sub-commands
	void handleScreenCreate(const std::string& args);   // screen -s <name> [<mem>]
	void handleScreenCustom(const std::string& args);   // screen -c <name> [<mem>] "<instrs>"
	void handleScreenResume(const std::string& name);   // screen -r <name>
	bool validMemorySize(std::size_t bytes) const;      // power of 2 within [2^6, 2^16]

	// process screen handlers
	void handleProcessScreenCommand(const std::string& input);
	void displayProcessSMI();

	// Shared output (used by - screen -ls - report-util)
	// maxRows caps each section so a stress run with thousands of processes stays
	// readable (and does not lock up the Windows console). 0 = print everything,
	// which is what csopesy-log.txt gets.
	static constexpr std::size_t CONSOLE_LIST_ROWS = 30;
	void printProcessTable(std::ostream& out, std::size_t maxRows = 0);

	//Utility
	std::string getCurrentTimestamp();
};
