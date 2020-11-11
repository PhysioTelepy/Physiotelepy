#pragma once

#include <string.h>
#include <iostream>

class UserInteraction
{
public:
	static int askInt(std::string msg, int min_value, int max_value);
	static int askIntDefault(std::string msg, int max_value, int min_value, int default_value);
	static std::string askString(std::string msg);
	static bool confirm(std::string message);
};