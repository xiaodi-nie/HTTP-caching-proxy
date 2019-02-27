#ifndef __PROXY_SERVER__H__
#define __PROXY_SERVER__H__

#include <algorithm>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include <pthread.h>
#include "cache.h"
#include "http_struct.h"

using namespace std;
class Proxy_server{
    int client_sock_fd;
    string request;
    int port;
    string host_ip;
public:
Proxy_server(int client_fd, string h): client_sock_fd(client_fd), host_ip(h){}
    /*create server socket and build connection with server***/
    int build_sock_connection(string host, string port, int client_sock_fd, int thread_id){
        int status;
        int socket_fd;
        struct addrinfo host_info;
        struct addrinfo *host_info_list;
        memset(&host_info, 0, sizeof(host_info));
        host_info.ai_family   = AF_UNSPEC;
        host_info.ai_socktype = SOCK_STREAM;
        status = getaddrinfo(host.c_str(), port.c_str(), &host_info, &host_info_list);
        if (status != 0) {
            cerr << "Error: cannot get address info for host" << endl;
            return -1;
        }
        socket_fd = socket(host_info_list->ai_family,
                           host_info_list->ai_socktype,
                           host_info_list->ai_protocol);
        if (socket_fd == -1) {
            cerr << "Error: cannot create socket" << endl;
            return -1;
        }
        status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1) {
            cout << thread_id << endl;
            cerr << "Error: cannot connect to socket" << endl;
            close(socket_fd);
            return -1;
        }
        return socket_fd;
    }

    /*send request to server and get response, return the response string****/
    string get_http_response(Http_struct http_struct, string request, int socket_fd, int thread_id, ofstream & file){
        int numbytes;
        string response;
        size_t response_line_position;
        if((numbytes = send(socket_fd, request.c_str(), request.length(), 0)) == -1){
            perror("Potato Send to left nei: ");
        }
        //cout <<"Bytes send to server "<< numbytes << endl;
        int message_length;
        int buffersize = 100;
        std::vector<char> buffer(buffersize);
        vector<char> response_v(buffersize);
        do{
            char *p = buffer.data();
            if((message_length = recv(socket_fd, p, buffersize, 0)) == -1){
                perror("receive error, socket-closed");
            }
            //cout << endl;
            //cout <<"*********"<<endl;
            //cout << message_length << endl;
            //cout << message_length << endl;
            string temp(buffer.begin(), buffer.begin() + message_length);
            //cout << temp << endl;
            //cout <<"*********"<<endl;
            //cout << endl;
            response.append(temp);
            int response_header_length = http_struct.find_header_length(response);
            //check if we receive header
            if(response_header_length != -1){
                //check if the response is chunked or not
                int response_content_length =  http_struct.find_content_length(response);
                //chunked message
                if(response.find("chunked", 0) != string::npos){
                    if(response.find("0\r\n\r\n", 0) != string::npos){
                        break;
                    }
                    buffer.clear();
                    buffer.resize(1000);
                    buffersize = 1000;
                }
                //content_length
                else if(response_content_length != -1){
                    size_t end_of_response = response_content_length + response_header_length;
                    if(response.length() >= end_of_response){
                        break;
                    }
                    // we didn't receive all response keep resize
                    else{
                        buffer.clear();
                        buffer.resize(end_of_response);
                        buffersize = end_of_response;
                    }
                }
                //error
                else{
                    perror("ERROR, Here is the problem");
                }
            }
            //didn't receive header yet, resize buffer
            else{
                if(message_length == buffersize){
                    buffer.clear();
                    buffer.resize(message_length + buffersize);
                    buffersize += message_length;
                }
            }
        }while(message_length != 0);

        if(response.find("\r\n\r\n", 0) == string::npos){
            return "HTTP/1.1 502 Bad Gateway\r\n\r\n";
        }
        string response_line;
        if((response_line_position = response.find("\r\n", 0)) != string::npos){
            response_line = response.substr(0, response_line_position);
            file << thread_id << ": Received \"" << response_line << "\" from " << http_struct.host<< endl;
        }
        //cout << endl;
        //cout << "not in cache response" << endl;
        //cout << response << endl;
        // vector<char> v(response.begin(), response.end());
        if((numbytes=send(client_sock_fd, response.c_str(), response.length(), 0))==-1){
            perror("send error");
        }
        if(close (client_sock_fd) == -1){
            perror("client close error ");
        }
        if(close (socket_fd) == -1){
            perror("proxy close error ");
        }
        return response;
    }

    /*process CONNECT request
     *create tunnel and pass information between browser and server****/
    void process_connect(int server_sock_fd, int client_sock_fd, ofstream & file, int thread_id){
        string ok_message("HTTP/1.1 200 OK\r\n\r\n");
        int status;
        status = send(client_sock_fd,ok_message.c_str(),strlen(ok_message.c_str()),0);
        if(status < 0){
            perror("send ok to client");
            close(client_sock_fd);
            close(server_sock_fd);
            return;
        }
        int buffersize = 10000;
        vector<char> buffer(buffersize);
        fd_set read_fds;
        while(true){
            int max_fd = server_sock_fd;
            FD_ZERO(&read_fds);
            FD_SET(server_sock_fd,&read_fds);
            FD_SET(client_sock_fd,&read_fds);
            if(client_sock_fd > max_fd){
                max_fd = client_sock_fd;
            }
            select(max_fd + 1, &read_fds, NULL, NULL, NULL);
            int recv_sock_fd;
            int send_sock_fd;
            if(FD_ISSET(client_sock_fd, &read_fds)){
                recv_sock_fd = client_sock_fd;
                send_sock_fd = server_sock_fd;
            }
            else if(FD_ISSET(server_sock_fd, &read_fds)){
                recv_sock_fd = server_sock_fd;
                send_sock_fd = client_sock_fd;
            }
            char * p = buffer.data();
            int recv_length = recv(recv_sock_fd, p, buffersize, 0);
            if(recv_length <= 0){
                break;
            }
            int send_length = send(send_sock_fd, p, recv_length, 0);
            if(send_length <= 0){
                break;
            }
        }
        file << thread_id << ": Tunnel closed" << endl;
        close(client_sock_fd);
        close(server_sock_fd);
        return;
    }

    /*thread function***/
    void process(Cache &cache, int thread_id){
        get_http_request(cache , thread_id);
        return;
    }

    /*build connection with browser and receive request
     *process GET, POST and CONNECT seperately
     *if request is GET, process through cache***/
    void get_http_request(Cache &cache, int &thread_id){
        std::ofstream file;
        Http_struct my_http_struct;
        int message_length;
        int buffersize = 100;
        vector<char> buffer(buffersize);
        do{
            char * p = buffer.data();
            if((message_length = recv(client_sock_fd, p, buffersize, 0)) == -1){
                perror("receive error");
            }
            string temp(buffer.begin(), buffer.begin() + message_length);
            request.append(temp);
            int request_header_length = my_http_struct.find_header_length(request);
            //find header
            if( request_header_length != -1){
                // post content-length
                int request_content_length = my_http_struct.find_content_length(request);
                if(request_content_length != -1){
                    size_t end_of_request = request_content_length + request_header_length;
                    if(request.length() >= end_of_request){
                        break;
                    }
                }
                //post chunked
                else if (request.find("chunked" , 0) != string::npos){
                    if(request.find("0\r\n\r\n", 0) != string::npos){
                        break;
                    }
                    buffer.clear();
                    buffer.resize(1000);
                    buffersize = 1000;
                }
                else{
                    break;
                }
            }
            else{
                if(message_length == buffersize){
                    buffer.clear();
                    buffer.resize(message_length + buffersize);
                    buffersize += message_length;
                }
            }
        }while(message_length != 0);
        if(request.length() == 0){
	  //cout <<"empty thread, return and close socket " <<endl;
            close(client_sock_fd);
            return;
        }
        /*cout << endl;
        cout << thread_id << endl;
        cout << request << endl;
        cout << endl;*/
        my_http_struct.request_info(request);

        if(request.find("\r\n\r\n", 0) == string::npos || request.find("Host:", 0) == string::npos || host_ip == my_http_struct.host){
            string error_400 = "HTTP/1.1 400 Bad Request\r\n\r\n";
            int status;
            status = send(client_sock_fd, error_400.c_str(), strlen(error_400.c_str()), 0);
            if(status < 0){
                perror("send 400 to client");
            }
            close(client_sock_fd);
            return;
        }
        time_t currtime = time(NULL);
        file.open("/var/log/erss/proxy.log",std::ios::app);
        file << thread_id << ": " <<"\"" << my_http_struct.request_line << "\"" << " from " << host_ip << " @ " << asctime(gmtime(&currtime));
        //cout << request << endl;
        //cout << my_http_struct.host <<endl;
        //cout << my_http_struct.port << endl;
        int server_sock_fd = build_sock_connection(my_http_struct.host, my_http_struct.port,client_sock_fd, thread_id);
        if(server_sock_fd == -1){
            return;
        }
        if(my_http_struct.method == "POST"){
            get_http_response(my_http_struct, request, server_sock_fd, thread_id, file);
        }
        else if(my_http_struct.method == "GET"){
            //if no-store identified, forward request to server
            if(cache.parse_Cacheability_no_store(request) != string::npos){
                file <<thread_id<<": NOTE Request_cache-control: no-store " << endl;
                file <<thread_id<<": Requesting \"" << my_http_struct.request_line << "\" from " << my_http_struct.host << endl;
                get_http_response(my_http_struct, request, server_sock_fd, thread_id, file);
            }
            else{
                string response = cache.search_cache(request, my_http_struct, server_sock_fd, thread_id, file);
                if(response.empty()){
                    file << thread_id <<": not in cache" << endl;
                    file <<thread_id<<": Requesting \"" << my_http_struct.request_line << "\" from " << my_http_struct.host << endl;
                    response = get_http_response(my_http_struct, request, server_sock_fd, thread_id, file);
                    //update cache
                    if(response.find("200 OK", 0) != string::npos){
                        cache.update_cache(request, response, my_http_struct, thread_id, file);
                    }
                }
                else{//in cache
                    //cout << thread_id <<": in cache, vaild" <<endl;
                    send(client_sock_fd, response.c_str(),response.length(), 0);
                    close(client_sock_fd);
                    close(server_sock_fd);
                    return;
                }
            }
            file.close();
        }
        else if(my_http_struct.method == "CONNECT"){
            process_connect(server_sock_fd, client_sock_fd, file, thread_id);
        }
        else{
            cout << "can't handle this method" <<endl;
	    close(client_sock_fd);
	    close(server_sock_fd);
	    return;
        }
    }
};
#endif
