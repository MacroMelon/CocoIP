//
// Created by thari on 4/3/2020.
//

#include "ipConnection.h"

int ipConnection::listenAndAcceptClient(int port) {

    listenSock = socket(AF_INET, SOCK_STREAM, 0);     //create the listening socket and add FD to FD table
    if (listenSock < 0){
        #ifdef debugOutput
            std::cout<<"Error creating Listening socket"<<std::endl;
        #endif
        return -1;
    }
    else{
        #ifdef debugOutput
            std::cout<<"Successfuly created listening socket!"<<std::endl;
        #endif
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));      //zero serv_addr

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){       //bind listenenr to adrress
        #ifdef debugOutput
            std::cout<<"Listenner bind failed"<<std::endl;
        #endif
        return -2;
    }
    else{
        #ifdef debugOutput
            std::cout<<"Listenner successfully bound!"<<std::endl;
        #endif
    }

    listen(listenSock, 5);          //listen for connections

    clilen = sizeof(cli_addr);
    connSock = accept(listenSock, (struct sockaddr *) &cli_addr, (socklen_t*)&clilen);
    if (connSock<0){
        #ifdef debugOutput
            std::cout<<"Error on accepting client and creating new socket"<<std::endl;
		#endif
		close(listenSock);
        return -3;
    }
    else{
        #ifdef debugOutput
            std::cout<<"Client connection successful!"<<std::endl;
        #endif
    }
    close(listenSock);
    return 1;
}

int ipConnection::checkDataAvail() {
	int count;
	ioctl(connSock, FIONREAD, &count);
	return count;
}

int ipConnection::sendData(char *buff, int len){
    n = write(connSock,buff,len);
    return n;
}

int ipConnection::readData(char *data, int len) {
    bzero(data,len);
    n = read(connSock,data,len);
    return n;
}

int ipConnection::cleanUp() {
    #ifdef debugOutput
        std::cout << "Closing connections" << std::endl;
    #endif
    close(connSock);
    return 0;
}


