#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <thread>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mutex>
#include "proxy_server.h"


Cache myCache;
void process_request(Proxy_server server,int thread_id){
  server.process(myCache , thread_id);
  return;
}

int main(){
    const char *hostname = NULL;
    const char *port = "12345";
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res, *p;
    int sockfd, new_fd;
    int status;
    int yes = 1;
    //char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((status = getaddrinfo(hostname, port, &hints, &res)) != 0){
        cerr << "Error: cannot get address info for host" << endl;
        cerr << "  (" << hostname << "," << port << ")" << endl;
        return -1;
    }

    for(p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return -1;
    }

    if (listen(sockfd, 1000) == -1) {
        perror("listen");
        exit(1);
    }
    string client_ip;
    int thread_id = 0;
    while(1){
        thread_id++;
        addr_size = sizeof(client_addr);
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        client_ip = inet_ntoa(((struct sockaddr_in *)&client_addr)->sin_addr);
        Proxy_server server(new_fd, client_ip);
        //each process need to close their new_fd
        thread(process_request, server, thread_id).detach();
    }
    close(sockfd);
    return 0;
}