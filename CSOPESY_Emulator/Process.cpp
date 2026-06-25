#include "Process.h"

// TEMPLATE Member 2 replaces this

Process::Process(const std::string& name, int pid, int totalLines, const std::string& createdAt)
    : name(name), pid(pid), totalLines(totalLines), createdAt(createdAt) {
}

void Process::executeNextInstruction() {
    // Temp: just increment the line counter
    if (!isFinished) {
        currentLine++;
        if (currentLine >= totalLines)
            isFinished = true;
    }
}