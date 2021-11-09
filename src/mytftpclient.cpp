#define DEFAULT_BLOCK_SIZE 512

#include <iostream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <chrono>
#include <vector>
#include "cxxopts.hpp"
#include "arguments.hpp"
#include "udp.hpp"
#include "tftp.hpp"

void printTimestamp();
void printError(std::string error);
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
int main()
{
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
            CustomArgLine separated = CustomArgLine(line);

            // Process user input
            auto argumentsResult = setupArguments(separated);

            UDP connection;
            TFTP tftp;
            ServerConfig serverConfig = parseServerConfig(argumentsResult["a"].as<std::string>());
            std::string mode = argumentsResult["c"].as<std::string>();
            std::string filePath = requiredArgumentGet<std::string>(argumentsResult, "d");

            printTimestamp();
            std::cout << "Creating connection to server " << serverConfig.server << " port " << serverConfig.port << std::endl;
            connection.createSocket(serverConfig.server, serverConfig.port);

            int minimalMTU = connection.getMinimalMTU();
            int blocksize = DEFAULT_BLOCK_SIZE;
            if (argumentsResult.count("s") == 1)
            {
                blocksize = std::min(argumentsResult["s"].as<int>(), minimalMTU);
                printTimestamp();
                std::cout << "Minimal MTU of all network interfaces is " << minimalMTU << ". Blocksize set to " << blocksize << std::endl;
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
                printTimestamp();
                std::cout << "Sending read file request with " << mode << " mode" << std::endl;
                std::string rrq = tftp.makeRRQ(filePath, mode); //TODO transfersize
                connection.send(rrq);

                std::ofstream file(filePath);
                char *buffer = new char[blocksize];
                int recvBytesCount = 0;

                char *lastMessage = new char[blocksize];
                int lastRecvBytesCount = 0;
                int lastBlockNumber = 0;
                std::string lastSentAck;
                do
                {
                    // Receive file and check for prevoius ACK
                    recvBytesCount = connection.receive(buffer, blocksize);

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
                        std::cout << "Send " << sentBytes << " bytes ACK to block " << blockNumberString << std::endl;
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
                } while (recvBytesCount == blocksize);
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
        quit = true;
    }
    return 0;
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