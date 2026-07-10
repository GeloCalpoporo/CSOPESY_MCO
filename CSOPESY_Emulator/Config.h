#pragma once
#include <string>

// Shared config struct - used by ConsoleManager and Scheduler
struct Config {
    int          num_cpu = 1;
    std::string  scheduler = "rr";
    unsigned int quantum_cycles = 5;
    unsigned int batch_process_freq = 1;
    unsigned int min_ins = 100;
    unsigned int max_ins = 200;
    unsigned int delay_per_exec = 0;
};