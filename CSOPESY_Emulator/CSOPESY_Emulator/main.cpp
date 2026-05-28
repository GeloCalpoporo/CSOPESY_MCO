#include <iostream>
#include <fstream>
#include <string>

struct Config {
    int num_cpu;
    std::string scheduler;
    unsigned int quantum_cycles;
    unsigned int batch_process_freq;
    unsigned int min_ins;
    unsigned int max_ins;
    unsigned int delay_per_exec;
};

Config g_config;
bool isInitialized = false;

void loadConfig() {
    isInitialized = false;

    std::ifstream file("config.txt");
    if (!file.is_open()) {
        std::cout << "Error: config.txt not found. System could not initialize." << std::endl;
        return;
    }

    int temp_cpu = -1;
    std::string temp_sched = "";

    std::string key;
    while (file >> key) {
        if (key == "num-cpu")            file >> g_config.num_cpu;
        else if (key == "scheduler") {
            file >> g_config.scheduler;
            if (g_config.scheduler.length() >= 2 && g_config.scheduler.front() == '"') {
                g_config.scheduler = g_config.scheduler.substr(1, g_config.scheduler.length() - 2);
            }
        }
        else if (key == "quantum-cycles")      file >> g_config.quantum_cycles;
        else if (key == "batch-process-freq")  file >> g_config.batch_process_freq;
        else if (key == "min-ins")            file >> g_config.min_ins;
        else if (key == "max-ins")            file >> g_config.max_ins;
        else if (key == "delay-per-exec")      file >> g_config.delay_per_exec;
    }
    file.close();

    if (g_config.num_cpu < 1 || g_config.num_cpu > 128) {
        std::cout << "Error: Invalid num-cpu (" << g_config.num_cpu << "). Must be 1-128." << std::endl;
        return;
    }

    if (g_config.scheduler != "rr" && g_config.scheduler != "fcfs") {
        std::cout << "Error: Invalid scheduler '" << g_config.scheduler << "'. Use 'rr' or 'fcfs'." << std::endl;
        return;
    }

    if (g_config.min_ins > g_config.max_ins) {
        std::cout << "Error: min-ins cannot be greater than max-ins." << std::endl;
        return;
    }

    isInitialized = true;

    std::cout << "Processor configuration initialized successfully." << std::endl;

    // 3. Print the results so you can see it worked!
    std::cout << "\n--- Configuration Loaded ---" << std::endl;
    std::cout << "Num CPU:      " << g_config.num_cpu << std::endl;
    std::cout << "Scheduler:    " << g_config.scheduler << std::endl;
    std::cout << "Quantum:      " << g_config.quantum_cycles << std::endl;
    std::cout << "Batch Freq:   " << g_config.batch_process_freq << std::endl;
    std::cout << "Min Ins:      " << g_config.min_ins << std::endl;
    std::cout << "Max Ins:      " << g_config.max_ins << std::endl;
    std::cout << "Delay:        " << g_config.delay_per_exec << std::endl;
    std::cout << "----------------------------\n" << std::endl;
}

void displayHeader() {
    std::cout << " _______  _______  _______  _______  _______  _______  __   __ "<< std::endl;
    std::cout << "|       ||       ||       ||       ||       ||       ||  | |  |"<< std::endl;
    std::cout << "|       ||  _____||   _   ||    _  ||    ___||  _____||  |_|  |"<< std::endl;
    std::cout << "|       || |_____ |  | |  ||   |_| ||   |___ | |_____ |       |"<< std::endl;
    std::cout << "|      _||_____  ||  |_|  ||    ___||    ___||_____  ||_     _|"<< std::endl;
    std::cout << "|     |_  _____| ||       ||   |    |   |___  _____| |  |   |  "<< std::endl;
    std::cout << "|_______||_______||_______||___|    |_______||_______|  |___|  "<< std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Welcome to CSOPESY Emulator!\n" << std::endl;
    std::cout << "Developers:" << std::endl;
    std::cout << "Tujan, Lucas Antonio V F." << std::endl; //Add niyo nalang names niyo dito guyss
    std::cout << "Alviar, Kelvin Audric T."  << std::endl; 
    std::cout << "\nLast updated: 05-15-2026" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
}

int main() {
    std::string input;
    displayHeader();

    while (true) {
        std::cout << "\nroot:\\> ";
        std::getline(std::cin, input);

        if (input == "exit") {
            break;
        }
        else if (input == "initialize") {
            loadConfig();
        }
        else if (input.substr(0, 6) == "screen") {
            if (!isInitialized) {
                std::cout << "Error: Please type 'initialize' first." << std::endl;
            } else {
                // We'll add sub-command logic (-s, -ls) here next
                std::cout << "Screen command recognized: " << input << std::endl;
            }
        }
        else if (input == "") {
            continue;
        }
        else {
            std::cout << "Command '" << input << "' not recognized." << std::endl;
        }
    }

    return 0;
}