#pragma once
#define MAX_BUFFER 2048
#include <netdb.h>
#include <exception>
#include <string>

class UDPException : public std::exception
{
    int whichErrno;
    std::string message;

public:
    UDPException(int whichErrno, std::string additionalMessage = "");
    const char *what() const throw();
};

class CustomException : public std::exception
{
    std::string message;
    public:
    CustomException(std::string message): message(message){}
    const char *what() const throw() { return message.c_str(); };
};

class UDP
{
    int sockFd = -1;
    bool opened = false;
    struct addrinfo *endpoint = nullptr;
    UDP(const UDP&) = delete;

public:
    UDP() {};
    ~UDP();
    int timeoutSeconds;
    int send(const char *sentData, std::size_t length);
    int send(std::string s);
    int sendWithTimeout(const char *sentData, std::size_t length, int timeout);
    int sendWithTimeout(std::string s, int timeout);
    void createTimeout(int timeout);
    int createSocket(std::string server, int port);
    int receive(char *buffer, int maxLength);
    int receiveWithTimeout(char *buffer, int maxLength, int timeout);
    int checkTimeout(char *receiveBuffer, int maxLength);
    int getMinimalMTU();
    int close();
};