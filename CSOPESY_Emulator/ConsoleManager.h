#pragma once

#include <string>
#include <memory>
#include <ostream>
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

	// process screen handlers
	void handleProcessScreenCommand(const std::string& input);
	void displayProcessSMI();

	// Shared output (used by - screen -ls - report-util)
	void printProcessTable(std::ostream& out);

	//Utility
	std::string getCurrentTimestamp();
};