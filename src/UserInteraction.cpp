#include <signal.h>
#include "UserInteraction.h"
#include <sstream>
#include <iomanip>


int UserInteraction::askInt(std::string msg, int minValue, int maxValue)
{
    int input = minValue - 1;
    std::string s;
    do
    {
        std::cout << msg;
        getline(std::cin, s);
        if (!(std::istringstream(s) >> input))
            input = minValue - 1;
    } while (!std::cin.fail() && (input < minValue || input >= maxValue));
    return input;
}

int UserInteraction::askIntDefault(std::string msg, int minValue, int maxValue, int defaultValue)
{
    int input = defaultValue - 1;
    while (input != defaultValue && (input < minValue || input >= maxValue))
    {
        std::cout << msg << " (default is " << defaultValue << "): ";
        std::string s;
        getline(std::cin, s);
        input = defaultValue;
        if (!s.empty())
        {
            if (!(std::istringstream(s) >> input))
                input = defaultValue - 1;
        }
    }
    return input;
}

std::string UserInteraction::askString(std::string msg)
{
    std::cout << msg;
    std::string input;
    getline(std::cin, input);
    return input;
}

bool UserInteraction::confirm(std::string msg)
{
    std::string input;
    std::cout << msg + " [y/n] ";
    std::cin >> input;
    return input == "y";
}