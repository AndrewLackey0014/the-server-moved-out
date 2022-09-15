#include "TCPRequestChannel.h"


using namespace std;


TCPRequestChannel::TCPRequestChannel (const std::string _ip_address, const std::string _port_no) {
    if(_ip_address == ""){//server
        int status;
        struct addrinfo hints;
        struct addrinfo *servinfo;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        status = getaddrinfo(NULL, _port_no.c_str(), &hints, &servinfo);
        sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
        bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
        listen(sockfd, 10);
        //socket
        //bind
        //listen
        freeaddrinfo(servinfo);
        
    }else{//client
        int status;
        struct addrinfo hints;
        struct addrinfo *servinfo;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        status = getaddrinfo(_ip_address.c_str(), _port_no.c_str(), &hints, &servinfo);
        sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
        connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
        freeaddrinfo(servinfo);
        //socket
        //connect
    }
}

TCPRequestChannel::TCPRequestChannel (int _sockfd) {
    sockfd = _sockfd;
}

TCPRequestChannel::~TCPRequestChannel () {
    close(sockfd);
}

int TCPRequestChannel::accept_conn () {
    struct sockaddr_storage storage;
    socklen_t addr_size = sizeof(storage);
    return accept(sockfd, (struct sockaddr*)&storage, &addr_size);
}

int TCPRequestChannel::cread (void* msgbuf, int msgsize) {
    return recv(sockfd, msgbuf, msgsize, 0);
}

int TCPRequestChannel::cwrite (void* msgbuf, int msgsize) {
    return send(sockfd, msgbuf, msgsize, 0);
}
