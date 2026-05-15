#include <iostream>
#include <string>
#include <vector>

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
    std::cout << "\nLast updated: 05-15-2026" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
}

int main() {
    std::string command;
    bool initialized = false;

    displayHeader();

    while (true) {
        std::cout << "root:\\> ";
        std::getline(std::cin, command);

        if (command == "exit") {
            break;
        }
        else if (command == "initialize") {
            std::cout << "Processor configuration initialized." << std::endl;
            initialized = true;
        }
        else if (command == "screen") {
            if (!initialized) {
                std::cout << "Error: Please type 'initialize' first." << std::endl;
            } else {
                std::cout << "Screen details shown here (placeholder)." << std::endl;
            }
        }
        else if (command == "") {
            // Do nothing if user just hits enter
            continue;
        }
        else {
            std::cout << "Command '" << command << "' not recognized." << std::endl;
        }
    }

    return 0;
}