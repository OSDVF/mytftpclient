#pragma once
#include <string>
class TFTP
{
    public:
        std::string makeRRQ(std::string filename, std::string mode = "binary", int blockSize = 512, int timeout = 0);
        std::string makeWRQ(std::string filename, std::string mode = "binary", int blockSize = 512, int transferSize = 0, int timeout = 0);
        char* makeData(int block, char* data);
        std::string makeACK(std::string block);
        std::string blockNumberToStr(int blockNumber);
};