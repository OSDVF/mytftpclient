#include "udp.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <cstring>
#include <string>
#include <sstream>

int UDP::send(std::string s)
{
    return send(s.c_str(), (int)(s.length() + 1)); //also send the null terminator
}
int UDP::send(const char *sentData, int length)
{
    int sentBytes;
    if ((sentBytes = sendto(sockFd, sentData, length, 0, endpoint->ai_addr, endpoint->ai_addrlen)) == -1)
    {
        throw UDPException(errno, " encountered while sending to server.");
    }

    return sentBytes;
}

int UDP::receive(char *buffer, int maxLength)
{
    int receivedBytes;
    if ((receivedBytes = recvfrom(sockFd, buffer, maxLength, 0, endpoint->ai_addr, &endpoint->ai_addrlen)) == -1)
    {
        throw UDPException(errno, "encountered while receiving from server.");
    }
    return receivedBytes;
}

int UDP::createSocket(std::string server, int port)
{
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    int returnValue;
    // Get interfaces associated with the address
    // Fills *servinfo
    if ((returnValue = getaddrinfo(server.c_str(), std::to_string(port).c_str(), &hints, &servinfo)) != 0)
    {
        throw UDPException(errno, gai_strerror(returnValue));
    }

    // loop through all the results and make a socket
    for (endpoint = servinfo; endpoint != NULL; endpoint = endpoint->ai_next)
    {
        if ((sockFd = socket(endpoint->ai_family, endpoint->ai_socktype, endpoint->ai_protocol)) == -1)
        {
            throw UDPException(errno, " encountered while creating socket");
        }
        break;
    }
    if (endpoint == NULL)
    {
        throw CustomException("Failed to bind socket");
    }
    opened = true;
    return sockFd;
}

int UDP::checkTimeout(char *receiveBuffer, int maxLength)
{
    fd_set fds;
    int n;
    struct timeval tv;

    // set up the file descriptor set
    FD_ZERO(&fds);
    FD_SET(sockFd, &fds);

    // set up the struct timeval for the timeout
    tv.tv_sec = timeoutSeconds;
    tv.tv_usec = 0;

    // wait until timeout or data received
    n = select(sockFd + 1, &fds, NULL, NULL, &tv);
    if (n == 0)
    {
        printf("timeout\n");
        return -2; // timeout!
    }
    else if (n == -1)
    {
        printf("error\n");
        return -1; // error
    }

    return recvfrom(sockFd, receiveBuffer, maxLength - 1, 0, endpoint->ai_addr, &endpoint->ai_addrlen);
}

int UDP::getMinimalMTU()
{
    ifreq ifr;
    int minimalMtu = INT32_MAX;
    int i = 1;
    while (true)
    {
        ifr.ifr_ifindex = i++;
        if (ioctl(sockFd, SIOCGIFNAME, &ifr) == -1)
        {
            return minimalMtu;
        }
        ioctl(sockFd, SIOCGIFMTU, &ifr);
        minimalMtu = std::min(ifr.ifr_mtu, minimalMtu);
    }
}

const auto& closeFd = close;//Rename the function
int UDP::close()
{
    opened = false;
    return closeFd(sockFd);
}
UDP::~UDP()
{
    if(opened)
        close();
}
UDPException::UDPException(int whichErrno, std::string additionalMessage)
{
    std::ostringstream msgBuilder("Network Error: ");
    msgBuilder << std::string(strerror(whichErrno)) << " ";
    msgBuilder << additionalMessage;
    this->message = msgBuilder.str();
    this->whichErrno = whichErrno;
}
const char *UDPException::what() const throw()
{
    return this->message.c_str();
}