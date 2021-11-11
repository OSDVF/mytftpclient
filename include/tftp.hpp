#pragma once
#include <string>
#include "udp.hpp"
class TFTP
{
    bool asciiMode = false;
    int &timeout;

public:
    /// Constructed with reference to timeout variable - because it can change in parent scope from time to time
    TFTP(int &timeout) : timeout(timeout){}
    std::string makeRRQ(std::string filename, std::string mode = "binary", int blockSize = 512, int timeoutOffer = 0);
    int sendRRQ(UDP& connection, std::string filename, std::string mode = "binary", int blockSize = 512, int timeoutOffer = 0);
    std::string makeWRQ(std::string filename, std::string mode = "binary", int blockSize = 512, int transferSize = 0, int timeoutOffer = 0);
    std::string makeACK(std::string block);
    std::string blockNumberToStr(int blockNumber);
    int netasciiToOctet(char *buffer, int length, bool &previous_cr);
    int octetToNetascii(char *buffer, char *outputWithDoubleCapacity, int length);
    int receive(UDP &connection, char *buffer, int maxLength, int& networkRecvBytes);
    int send(UDP& connection, int blockNumber, char *data, int length);
};