#pragma once
#include <string>
class TFTP
{
    public:
        std::string makeRRQ(std::string filename, std::string mode = "binary");
        std::string makeWRQ(std::string filename, std::string mode = "binary");
        char* makeData(int block, char* data);
        std::string makeACK(std::string block);
        std::string blockNumberToStr(int blockNumber);
};