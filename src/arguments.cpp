#include "arguments.hpp"
#include <iterator>

cxxopts::Options setupArguments()
{
    cxxopts::Options options("My TFTP Client", "Client for transferring files througn Trivial File Transfer Protocol");
        options.add_options("Required")
            ("R,Read", "Read file from server. Do not combine with -W")
            ("W,Write", "Write file to server. Do not combine with -R")
            ("d,file","File path", cxxopts::value<std::string>());
        options.add_options("Optional")
            ("t,timeout", "Timeout in seconds. 0 = no timeout", cxxopts::value<int>()->default_value("0"))
            ("s,size","Maximum block size. Default higher bound of block size is the smallest MTU", cxxopts::value<int>())
            ("m,multicast","Request multicast transfer. Not implemented yet.")
            ("c,code","Transfer mode. Can be \"ascii\" (or also \"netascii\") or \"binary\" (or also \"octet\").", cxxopts::value<std::string>()->default_value("binary"))
            ("a,address","Server address and port formatted: adress,port", cxxopts::value<std::string>()->default_value("127.0.0.1,69"));
        return options;
}

cxxopts::ParseResult parseArguments(CustomArgLine& separated)
{
    auto options = setupArguments();
    return options.parse(separated.count, separated.separated);;
}

CustomArgLine::~CustomArgLine()
{
    if (count != 0)
    {
        count = 0;
        for (int i = 0; i < count; i++)
        {
            delete[] separated[i];
        }
        delete[] separated;
        separated = nullptr;
    }
}
CustomArgLine::CustomArgLine(std::string line)
{
    std::istringstream iss(line);
    std::vector<std::string> words{std::istream_iterator<std::string>{iss},
                      std::istream_iterator<std::string>{}};
    this->count = words.size() + 1; //The 0-th argv member is always the name of the program
    this->separated = new char *[count];
    //Construct 0-th member
    this->separated[0] = new char[1];
    this->separated[0][0] = 0;
    for (int i = 1; i < count; i++)
    {
        auto& word = words[i-1];
        auto len = word.length();
        this->separated[i] = new char[len + 1];
        strncpy(this->separated[i], word.c_str(), len + 1);
    }
}