#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <ctime>

class ThreadDebug
{
public:
	ThreadDebug(const std::string& name) : _name(name) {}

	std::string _name;

	inline std::ostream& print() 
	{
		time_t t = time(0);
		struct tm* now = localtime(&t);
		return std::cout << "[" << _name << " | " << std::setfill('0') << std::setw(2) << now->tm_hour << ":" << std::setw(2) << now->tm_min << ":" << std::setw(2) << now->tm_sec << "] ";
	}
};
