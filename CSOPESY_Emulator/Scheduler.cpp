#include "Scheduler.h"
#include <algorithm>

// ================================================================
// all Temporary implementations are here, for Member 3 to implement Scheduler.cpp
// ================================================================

void Scheduler::initialize(const Config& config) {
    numCores = config.num_cpu;
}

void Scheduler::startGeneration() {}

void Scheduler::stopGeneration() {}

std::shared_ptr<Process> Scheduler::createProcess(const std::string& name) {
    // Reject duplicate names
    if (findProcess(name) != nullptr) {
        return nullptr;
    }
    // Temp: just make a dummy process so ConsoleManager can test screens
	auto p = std::make_shared<Process>(name, nextPid++, 100, "01/01/2026 12:00:00AM"); // Hardcoded timestamp for now
    p->coreId = 0;
    processes.push_back(p);
    return p;
}

std::shared_ptr<Process> Scheduler::findProcess(const std::string& name) {
    for (auto& p : processes) {
        if (p->name == name) return p;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Process>> Scheduler::getAllProcesses() {
    return processes;
}

int Scheduler::getCoresUsed() const {
    int count = 0;
    for (const auto& p : processes)
        if (!p->isFinished) count++;
    return std::min(count, numCores);
}

int Scheduler::getCoresAvailable() const {
    return numCores - getCoresUsed();
}

double Scheduler::getCpuUtilization() const {
    if (numCores == 0) return 0.0;
    return (getCoresUsed() / (double)numCores) * 100.0;
}