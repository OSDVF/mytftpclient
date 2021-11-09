#include "tftp.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>

std::string TFTP::blockNumberToStr(int blockNumber)
{
    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << blockNumber;
    std::string s = ss.str();
    return s;
}

std::string TFTP::makeRRQ(std::string filename, std::string mode)
{
    std::ostringstream ss;
    ss << '\000' << '\001';
    ss.write(filename.c_str(), filename.length()+1);//Write also the null terminator
    ss << mode;

    return ss.str();
}

std::string TFTP::makeWRQ(std::string filename, std::string mode)
{
    std::ostringstream ss;
    ss << '\000' << '\002';
    ss.write(filename.c_str(), filename.length()+1);
    ss << mode;

    return ss.str();
}

char *TFTP::makeData(int block, char *data)
{
    char *packet;
    auto bn = blockNumberToStr(block);
    auto size = 4 + strlen(data);
    packet = (char *)malloc(size);
    memset(packet, 0, size);
    strcat(packet, "\000\003"); //opcode
    strcat(packet, bn.c_str());
    strcat(packet, data);
    return packet;
}

std::string TFTP::makeACK(std::string block)
{
    std::ostringstream ss;
    ss << '\000' << '\004';
    ss.write(block.c_str() ,2);
    return ss.str();
}