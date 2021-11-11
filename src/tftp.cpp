#include "tftp.hpp"
#include "udp.hpp"
#include <cstring>
#include <sstream>
#include <iomanip>

void writeOptions(std::ostringstream &ss, int blockSize, int transferSize, int timeoutOffer);
void writeOption(std::ostringstream &ss, int option, std::string name);

std::string TFTP::blockNumberToStr(int blockNumber)
{
    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << blockNumber;
    std::string s = ss.str();
    return s;
}

// https://github.com/reinerh/rtftp/blob/trunk/src/lib.rs
int TFTP::netasciiToOctet(char *buffer, int length, bool &previous_cr)
{
    int resultLength = 0;
    for (int i = 0; i < length; i++)
    {
        auto b = buffer[i];
        switch (b)
        {
        case '\r':
            if (previous_cr)
            {
                buffer[i] = '\r';
                resultLength++;
            }
            previous_cr = true;
            continue;

        case '\0':
            if (previous_cr)
            {
                buffer[i] = '\r';
                resultLength++;
            }
            break;
        case '\n':
            if (previous_cr)
            {
                buffer[i] = '\n';
                resultLength++;
            }
        default:
            resultLength++;
            break;
        }

        previous_cr = false;
    }
    return resultLength;
}

int TFTP::octetToNetascii(char *buffer, char *outputWithDoubleCapacity, int length)
{
    int resultLength = 0;
    for (int i = 0; i < length; i++)
    {
        auto &out = outputWithDoubleCapacity[i];
        switch (buffer[i])
        {
        case '\r':
            out = '\r';
            outputWithDoubleCapacity[++i] = '\0';
            resultLength += 2;
            break;

        case '\n':
            out = '\r';
            outputWithDoubleCapacity[++i] = '\n';
            resultLength += 2;
            break;
        default:
            out = buffer[i];
            resultLength++;
            break;
        }
    }
    return resultLength;
}

void writeOption(std::ostringstream &ss, int option, std::string name)
{
    ss << '\0';
    ss.write(name.c_str(), name.length() + 1);
    auto stringValue = std::to_string(option);
    ss << stringValue;
}

void writeOptions(std::ostringstream &ss, int blockSize, int transferSize, int timeoutOffer)
{
    writeOption(ss, blockSize, "blksize");
    if (timeoutOffer != 0)
    {
        writeOption(ss, timeoutOffer, "timeout");
    }
    writeOption(ss, transferSize, "tsize");
}

std::string TFTP::makeRRQ(std::string filename, std::string mode, int blockSize, int timeoutOffer)
{
    std::ostringstream ss;
    ss << '\000' << '\001';
    ss.write(filename.c_str(), filename.length() + 1); //Write also the null terminator
    ss << mode;

    if (mode == "ascii" || mode == "netascii")
    {
        this->asciiMode = true;
    }

    writeOptions(ss, blockSize, 0, timeoutOffer);

    return ss.str();
}

int TFTP::sendRRQ(UDP &connection, std::string filename, std::string mode, int blockSize, int timeoutOffer)
{
    if (timeoutOffer == 0)
    {
        return connection.send(makeRRQ(filename, mode, blockSize, timeoutOffer));
    }
    return connection.sendWithTimeout(makeRRQ(filename, mode, blockSize, timeoutOffer), timeoutOffer);
}

std::string TFTP::makeWRQ(std::string filename, std::string mode, int blockSize, int transferSize, int timeoutOffer)
{
    std::ostringstream ss;
    ss << '\000' << '\002';
    ss.write(filename.c_str(), filename.length() + 1);
    ss << mode;

    if (mode == "ascii" || mode == "netascii")
    {
        this->asciiMode = true;
    }

    writeOptions(ss, blockSize, transferSize, timeoutOffer);

    return ss.str();
}

int TFTP::send(UDP &connection, int blockNumber, char *data, int length)
{
    std::ostringstream ss;
    auto bn = blockNumberToStr(blockNumber);
    ss << "\000\003"; //opcode
    ss.write(bn.c_str(), 2);
    if (this->asciiMode)
    {
        char *outData = new char[length * 2];
        int outDataLength = octetToNetascii(data, outData, length);
        ss.write(outData, outDataLength);
        delete[] outData;
    }
    else
    {
        ss.write(data, length);
    }

    if (this->timeout == 0)
    {
        return connection.send(ss.str());
    }
    return connection.sendWithTimeout(ss.str(), timeout);
}

std::string TFTP::makeACK(std::string block)
{
    std::ostringstream ss;
    ss << '\000' << '\004';
    ss.write(block.c_str(), 2);
    std::string str = ss.str();
    return str;
}

int TFTP::receive(UDP &connection, char *buffer, int maxLength, int& networkRecvBytes)//Returns number of bytes in the converted message
{
    if (this->timeout == 0)
    {
        networkRecvBytes = connection.receive(buffer, maxLength);
    }
    else
    {
        networkRecvBytes = connection.receiveWithTimeout(buffer, maxLength, timeout);
    }

    if (this->asciiMode)
    {
        bool netasciiState = false;
        int resultLength = netasciiToOctet(buffer + 4, networkRecvBytes - 4, netasciiState);
        return resultLength;
    }
    else
    {
        return networkRecvBytes;
    }
}