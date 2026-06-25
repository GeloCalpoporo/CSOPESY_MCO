#pragma once

#include <memory>
#include <vector>
#include <string>
#include "Config.h"
#include "Process.h"

// ================================================================
// Scheduler.h  —  For Member 3 implements Scheduler.cpp
// ================================================================
// ConsoleManager calls these methods. Member 3 owns all thread logic,
// the CPU tick loop, core allocation, and process generation.
// DO NOT change the public interface without coordinating.
// ================================================================

class Scheduler {
public:
	// Called once after config.txt is loaded successfully
	void initialize(const Config& config);

	//Called by "scheduler-start" / "scheduler-stop"
	void startGeneration();
	void stopGeneration();

	//Called by "screen -s <name>": creates a new process and queues it
	//Returns nullpts if creation fails (e.g. name already exists)
	std::shared_ptr<Process> createProcess(const std::string& name);

	//Called by "screen -r <name>": looks up existing process by name
	//Returns nullptr if not found
	std::shared_ptr<Process> findProcess(const std::string& name);
	
	//Returns ALL processes (running + finished) for screen -ls / report util
	std::vector<std::shared_ptr<Process>> getAllProcesses();

	// CPU stats for screen -ls / report-util
	int getCoresUsed()         const;
	int getCoresAvailable()    const;
	double getCpuUtilization() const; //0.0 to 100.0

private:
	// Member 3 will expand this section heavily
	std::vector<std::shared_ptr<Process>> processes;
	int numCores = 1;
	int nextPid = 1;
};