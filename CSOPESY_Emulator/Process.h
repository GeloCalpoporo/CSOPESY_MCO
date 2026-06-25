#pragma once
#include <string>
#include <vector>
#include <mutex>

// ================================================================
// Process.h  —  For Member 2 implements Process.cpp
// ================================================================
// ConsoleManager and Scheduler both hold shared_ptr<Process>.
// Fill in the constructor body and executeNextInstruction().
// DO NOT change the public fields/methods below without coordinating.
// ================================================================

class Process {
public:
	// Identifier/Identity Process Name ID 
	std::string name; 
	int pid = 0; 

	// Instruction Tracking (program Counter)
	int currentLine = 0; // # of instructions that have been executed so far
	int totalLines = 0;  // Total # of instructions in the process 
	
	// Runtime state 
	int coreId = -1; 
	bool isFinished = false;

	// Timestamp (set at creation)
	std::string createdAt; // format is: MM/DD/YYYY HH:MM:SSAM

	// Print log (output of -PRINT- instrcutions)
	// Scheduler/Process writes here; ConsoleManager reads here for process-smi
	std::vector<std::string> printLogs;
	std::mutex logMutex; // Lock before touching printLogs
	
	// Constructor
	// Member 2 will implement here, body will be in Process.cpp
	Process(const std::string& name, int pid, int totalLines, const std::string& createdAt);

	// Called by Scheduler once per CPU tick
	// Member 2 will implement here, execute one instruction, advances currentLine
	// sets isFinished to true if currentLine >= totalLines, or when all instructions are executed/done
	void executeNextInstruction();
};	