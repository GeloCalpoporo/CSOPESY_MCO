#pragma once
#include <string>
#include <cstddef>

// Shared config struct - used by ConsoleManager, Scheduler and MemoryManager.
// Parameters are read from config.txt by ConsoleManager::loadConfig().
struct Config {
    // ----- MCO1: scheduler -----
    int          num_cpu            = 4;
    std::string  scheduler          = "rr";
    unsigned int quantum_cycles     = 5;
    unsigned int batch_process_freq = 1;
    unsigned int min_ins            = 1000;
    unsigned int max_ins            = 2000;
    unsigned int delay_per_exec     = 0;

    // ----- MCO2: memory -----
    // All four must be powers of 2 within [2^6, 2^16] bytes.
    std::size_t  max_overall_mem    = 16384;   // total simulated physical memory
    std::size_t  mem_per_frame      = 256;     // bytes per frame (== bytes per page)
    std::size_t  min_mem_per_proc   = 256;     // memory rolled for scheduler-start procs
    std::size_t  max_mem_per_proc   = 4096;
};
