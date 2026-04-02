//
// Created by thari on 4/4/2020.
//

#ifndef CTRL_VIA_IP_CONTROLLERCLIENT_SERVERCON_H
#define CTRL_VIA_IP_CONTROLLERCLIENT_SERVERCON_H

//#define debugOutput

#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef debugOutput
    #include <iostream>
#endif

#ifndef bufflen
    #define bufflen 512
#endif

class serverCon {
private:
    WSADATA wsaData;
    int iResult;
    struct addrinfo *server_addrInfo = NULL, *ptr = NULL, clientInfo;
    SOCKET serverConnection = INVALID_SOCKET;

public:
    int connectTo(char address[128], char port[8]);
    int sendData(char data[bufflen]);
    int readData(char buff[bufflen]);
    int cleanUp();

};

#endif //CTRL_VIA_IP_CONTROLLERCLIENT_SERVERCON_H