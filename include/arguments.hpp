#pragma once
#include "cxxopts.hpp"
struct CustomArgLine // Constructs argv-like array from a line string
{
    char **separated = nullptr;
    int count = 0;

    ~CustomArgLine();
    CustomArgLine(std::string line);

    CustomArgLine(const CustomArgLine&) = delete;
};

cxxopts::Options setupArguments();
cxxopts::ParseResult parseArguments(CustomArgLine&);