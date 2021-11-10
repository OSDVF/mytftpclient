#include "tftp.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>

void writeOptions(std::ostringstream &ss, int blockSize, int transferSize, int timeout);
void writeOption(std::ostringstream &ss, int option, std::string name);

std::string TFTP::blockNumberToStr(int blockNumber)
{
    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << blockNumber;
    std::string s = ss.str();
    return s;
}

void writeOption(std::ostringstream &ss, int option, std::string name)
{
    ss << '\0';
    ss.write(name.c_str(), name.length() + 1);
    auto stringValue = std::to_string(option);
    ss << stringValue;
}

void writeOptions(std::ostringstream &ss, int blockSize, int transferSize, int timeout)
{
    writeOption(ss, blockSize, "blksize");
    if (timeout != 0)
    {
        writeOption(ss, timeout, "timeout");
    }
    writeOption(ss, transferSize, "tsize");
}

std::string TFTP::makeRRQ(std::string filename, std::string mode, int blockSize, int timeout)
{
    std::ostringstream ss;
    ss << '\000' << '\001';
    ss.write(filename.c_str(), filename.length() + 1); //Write also the null terminator
    ss << mode;

    writeOptions(ss, blockSize, 0, timeout);

    return ss.str();
}

std::string TFTP::makeWRQ(std::string filename, std::string mode, int blockSize, int transferSize, int timeout)
{
    std::ostringstream ss;
    ss << '\000' << '\002';
    ss.write(filename.c_str(), filename.length() + 1);
    ss << mode;

    writeOptions(ss, blockSize, transferSize, timeout);

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
    ss.write(block.c_str(), 2);
    std::string str = ss.str();
    return str;
}