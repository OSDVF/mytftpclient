#define DEFAULT_BLOCK_SIZE 512

#include <iostream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <chrono>
#include <vector>
#include <sys/stat.h>
#include <sys/vfs.h>
#include "cxxopts.hpp"
#include "arguments.hpp"
#include "udp.hpp"
#include "tftp.hpp"

void printTimestamp();
void printError(std::string error);
long GetFileSize(std::string filename);
std::string base_name(std::string const &path);
bool checkOACKs(char *buffer, UDP &connection, int timeoutOffer, int &timeout, int blocksizeOffer, int &blocksize, long unsigned int &transferSize, bool read);
unsigned int stdStr2intHash(std::string str, int h = 0);
constexpr unsigned int str2intHash(const char *str, int h = 0)
{
    return !str[h] ? 5381 : (str2intHash(str, h + 1) * 33) ^ str[h];
}
struct ServerConfig
{
    std::string server;
    int port;
};
struct SkipToNextUserInput
{
};
template <typename T>
T requiredArgumentGet(cxxopts::ParseResult argumentsResult, std::string argumentName);
ServerConfig parseServerConfig(std::string confString);
template <typename T, typename U, typename V>
bool checkOptionError(T optionValue, U serverValue, V optionName);
int main()
{
    std::cout << "My TFTP Client. Enter 'q' to quit or 'h' for help." << std::endl;
    bool quit = false;
    while (!quit)
    {
        try
        {
            std::cout << "> ";

            // Scan user input
            std::string line;
            std::cin.clear();
            std::getline(std::cin, line);
            switch (stdStr2intHash(line))
            {
            case str2intHash("q"):
                quit = true;
                continue;
            
            case str2intHash("h"):
                std::cout << setupArguments().help();
                continue;
            }
            
            CustomArgLine separated = CustomArgLine(line);

            // Process user input
            auto argumentsResult = parseArguments(separated);

            UDP connection;
            TFTP tftp;
            ServerConfig serverConfig = parseServerConfig(argumentsResult["a"].as<std::string>());
            std::string mode = argumentsResult["c"].as<std::string>();
            std::vector<std::string> permittedModes = {"ascii", "octet", "netascii", "binary"};

            if (std::find(permittedModes.begin(), permittedModes.end(), mode) == permittedModes.end())
            {
                printError("Unknown transfer mode specified, but trying to use it...");
            }
            std::string filePath = requiredArgumentGet<std::string>(argumentsResult, "d");

            printTimestamp();
            std::cout << "Creating connection to server " << serverConfig.server << " port " << serverConfig.port << std::endl;
            connection.createSocket(serverConfig.server, serverConfig.port);

            int minimalMTU = connection.getMinimalMTU();
            int blockSizeOffer = DEFAULT_BLOCK_SIZE;
            int blocksize = DEFAULT_BLOCK_SIZE;
            long unsigned int transferSize = 0;
            int timeoutOffer = argumentsResult["t"].as<int>();
            int timeout = 0;
            if (argumentsResult.count("s") == 1)
            {
                blockSizeOffer = std::min(argumentsResult["s"].as<int>(), minimalMTU);
                printTimestamp();
                std::cout << "Minimal MTU of all network interfaces is " << minimalMTU << ". Blocksize set to " << blockSizeOffer << std::endl;
            }

            // BEGIN SERVER COMMUNICATION
            if (argumentsResult.count("R") == 1)
            {
                //Read branch
                if (argumentsResult.count("W"))
                {
                    printError("Do not combine Read and Write arguments.");
                    continue;
                }
                struct statfs64 fileSystemInfo;
                auto fileBaseName = base_name(filePath);
                std::ofstream file(fileBaseName);
                statfs64(fileBaseName.c_str(), &fileSystemInfo);//Get free disk space
                printTimestamp();
                std::cout << "There are " << fileSystemInfo.f_bsize * fileSystemInfo.f_bfree << " free bytes on disk" << std::endl;

                printTimestamp();
                std::cout << "Sending read file request with " << mode << " mode" << std::endl;
                std::string rrq = tftp.makeRRQ(filePath, mode, blockSizeOffer, timeoutOffer);
                connection.send(rrq);

                char *buffer = new char[std::max(blockSizeOffer, blocksize) + 4]; //+4 because 2 bytes for opcode and 2 bytes for the block number
                int recvBytesCount = 0;

                char *lastMessage = new char[std::max(blockSizeOffer, blocksize) + 4];
                int lastRecvBytesCount = 0;
                int lastBlockNumber = 0;
                std::string lastSentAck;
                bool gotOACK = false;
                do
                {
                    gotOACK = false;
                    recvBytesCount = connection.receive(buffer, blocksize + 4);

                    // Receive option acknowledgements (OACKs)
                    // This function also updates corresponding option values
                    if (checkOACKs(buffer, connection, timeoutOffer, timeout, blockSizeOffer, blocksize, transferSize, true))
                    {
                        //If received an OACK, server accepted the offer
                        //Continue with receiving
                        gotOACK = true;
                        printTimestamp();
                        if (fileSystemInfo.f_bfree * fileSystemInfo.f_bsize >= transferSize)//Check if there is enough disk space
                        {
                            std::cout << "Sending ACK to OACK" << std::endl;
                            connection.send(tftp.makeACK(std::string({'\0', '\0'})));
                        }
                        continue;
                    }

                    //Check for DATA packet opcode
                    std::string blockNumberString({static_cast<char>(buffer[2] + '0'), static_cast<char>(buffer[3] + '0')});
                    std::istringstream bntoInt(blockNumberString);
                    int blockNumber;
                    bntoInt >> blockNumber;
                    std::string packetOpcode({static_cast<char>(buffer[0] + '0'), static_cast<char>(buffer[1] + '0')});
                    printTimestamp();
                    std::cout << "Received " << recvBytesCount << " bytes packet with opcode " << packetOpcode << " with block number " << blockNumberString << std::endl;

                    //Check for error packet
                    if (packetOpcode == "05")
                    {
                        std::ostringstream errOutput;
                        errOutput << "Server send an error packet. Contents:" << std::endl;
                        errOutput.write(buffer + 4, recvBytesCount - 4);
                        printError(errOutput.str());
                        throw SkipToNextUserInput();
                    }
                    else if (packetOpcode != "03")
                    {
                        printError("Warning: Received packet which supposet to be DATA packet with unusual opcode");
                    }

                    if (blockNumber == lastBlockNumber + 1) //Block numbers should increase with 1
                    {
                        // WRITE to the file
                        file.write(buffer + 4, recvBytesCount - 4); //Because the first 4 bytes are the block number

                        // Send acknowledgment packet
                        std::string ackMessage = tftp.makeACK({buffer[2], buffer[3]});
                        int sentBytes = connection.send(ackMessage);
                        printTimestamp();
                        std::cout << "Sent " << sentBytes << " bytes ACK to block " << blockNumberString << std::endl;
                        lastSentAck = ackMessage;
                        std::memcpy(lastMessage, buffer, recvBytesCount);
                        lastBlockNumber = blockNumber;
                        lastRecvBytesCount = recvBytesCount;
                    }
                    else
                    {
                        if (recvBytesCount == lastRecvBytesCount && std::memcmp(buffer, lastMessage, recvBytesCount) == 0)
                        {
                            //SEND LAST ACK AGAIN - as it probably didn't reach the server
                            printTimestamp();
                            std::cout << "Sending ACK for " << lastBlockNumber << " again." << std::endl;
                            connection.send(lastSentAck);
                            continue; //Receive next block
                        }

                        // In this place the block number is out of sync, so we must abort the transfer
                        std::cerr << "Expected " << (lastBlockNumber + 1) << " but got " << blockNumber << std::endl;
                        printError("Block number out of sync.");
                    }
                } while (gotOACK || recvBytesCount == blocksize + 4 || recvBytesCount == blockSizeOffer + 4);
                delete[] buffer;
                delete[] lastMessage;
            }
            else if (argumentsResult.count("W") == 1)
            {
                //Write branch
                if (argumentsResult.count("R"))
                {
                    printError("Do not combine Read and Write arguments.");
                    continue;
                }
            }
            else
            {
                printError("Specify either -R for Read or -W for Write file mode.");
                continue;
            }

            connection.close();
            printTimestamp();
            std::cout << "Connection finished." << std::endl;
        }
        catch (const std::exception &e)
        {
            printError("An exception occured");
            printError(e.what());
            continue;
        }
        catch (const SkipToNextUserInput &e)
        {
            //Just continue
            continue;
        }
    }
    return 0;
}

std::string base_name(std::string const &path)
{
    //https://stackoverflow.com/questions/8520560/get-a-file-name-from-a-path
    return path.substr(path.find_last_of("/\\") + 1);
}

long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

unsigned int stdStr2intHash(std::string str, int h)
{
    return !str.c_str()[h] ? 5381 : (str2intHash(str.c_str(), h + 1) * 33) ^ str.c_str()[h];
}

bool checkOACKs(char *buffer, UDP &connection, int timeoutOffer, int &timeout, int blocksizeOffer, int &blocksize, long unsigned int &transferSize, bool read)
{
    if (buffer[0] == 0 && buffer[1] == 6) // 06 = OACK
    {
        int bufferPos = 2;
        int thisOptionLength;
        int thisValueLength;
        int thisValuePos;
        int nextOptionStartIndex;
        do
        {
            const char *optionName = buffer + bufferPos;
            thisOptionLength = std::strlen(optionName);

            thisValuePos = bufferPos + thisOptionLength + 1;
            const char *optionValue = buffer + thisValuePos;
            thisValueLength = std::strlen(optionValue);

            nextOptionStartIndex = std::min(thisValuePos + thisValueLength + 1, DEFAULT_BLOCK_SIZE - 1);

            auto optionValueString = std::string(optionValue);
            std::istringstream optionValueStream(optionValueString);
            switch (str2intHash(optionName))
            {
            case str2intHash("timeout"):
                if (checkOptionError(timeoutOffer, optionValueString, optionName))
                {
                    timeout = timeoutOffer;
                    printTimestamp();
                    std::cout << "Timeout accepted" << std::endl;
                }
                else
                {
                    timeout = 0;
                }
                break;

            case str2intHash("blksize"):
                if (checkOptionError(blocksizeOffer, optionValueString, optionName))
                {
                    blocksize = blocksizeOffer;
                    printTimestamp();
                    std::cout << "Block size accepted" << std::endl;
                }
                else
                {
                    blocksize = DEFAULT_BLOCK_SIZE;
                }
                break;

            case str2intHash("tsize"):
                if (read)
                {
                    optionValueStream >> transferSize;
                }
                else
                {
                    checkOptionError(transferSize, optionValueString, optionName);
                }
                printTimestamp();
                std::cout << "Transfer size accepted: " << transferSize << std::endl;
                break;
            case str2intHash("\n\b"):
                printTimestamp();
                std::cout << "End of option acknowledgements" << std::endl;
                break;

            default:
                std::ostringstream errOutput;
                errOutput << "Unknown option OACK: " << optionName;
                printError(errOutput.str());
                break;
            }
            //Advance to next option
            bufferPos = nextOptionStartIndex;
        } while (buffer[nextOptionStartIndex] != 0);
        return true;
    }
    return false;
}

ServerConfig parseServerConfig(std::string confString)
{
    ServerConfig result;
    std::stringstream test(confString);
    std::string segment;
    std::vector<std::string> seglist;

    if (std::getline(test, segment, ','))
    {
        result.server = segment;
    }
    test >> result.port;

    return result;
}

template <typename T, typename U, typename V>
bool checkOptionError(T optionValue, U serverValue, V optionName)
{
    if (serverValue != std::to_string(optionValue))
    {
        std::ostringstream errOut("Server did not recognize ");
        errOut << optionName << " (we sent " << optionValue << " and server replied " << serverValue << ")";
        printError(errOut.str());
        return false;
    }
    return true;
}

void printTimestamp()
{
    //https://stackoverflow.com/questions/24686846/get-current-time-in-milliseconds-or-hhmmssmmm-format
    // get current time
    auto now = std::chrono::system_clock::now();

    // get number of milliseconds for the current second
    // (remainder after division into seconds)
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // convert to std::time_t in order to convert to std::tm (broken time)
    auto time = std::chrono::system_clock::to_time_t(now);
    auto local = *std::localtime(&time);
    std::cout << '[' << std::put_time(&local, "%Y-%m-%d %H:%M:%S.") << std::setfill('0') << std::setw(3) << ms.count();
    std::cout << "] ";
}

void printError(std::string error)
{
    printTimestamp();
    std::cerr << error << std::endl;
}

template <typename T>
T requiredArgumentGet(cxxopts::ParseResult argumentsResult, std::string argumentName)
{
    int count;
    if ((count = argumentsResult.count(argumentName)) == 1)
    {
        return argumentsResult[argumentName].as<T>();
    }
    else
    {
        std::stringstream out;
        out << "Required argument \"" << argumentName << "\" was specified " << count << " times.";
        printError(out.str());
        throw SkipToNextUserInput();
    }
}