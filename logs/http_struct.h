#ifndef __HTTP_STRUCT__H__
#define __HTTP_STRUCT__H__
#include <string>

using namespace std;

class Http_struct{
public:
    string request;
    string method;
    string request_line;
    string host;
    string port;
    string URL;
public:
    Http_struct(){};
    void request_info(string req){
        request = req;
        parse_method();
        parse_host();
        parse_URL();
        parse_request_line();
    }

    void parse_URL(){
        int URL_position = request.find(" ", 0);
        int end_of_URL = request.find(" ", URL_position + 1);
        URL = request.substr(URL_position + 1, end_of_URL - URL_position -1);
    }
    void parse_method(){
        int method_position = request.find(" ", 0);
        method = request.substr(0, method_position);
    }
    void parse_request_line(){
        int request_line_position = request.find("\r\n", 0);
        request_line = request.substr(0, request_line_position);
    }
    void parse_host(){
        int host_position = request.find("Host: ", 0);
        int end_of_line = request.find("\r\n", host_position);
        host = request.substr(host_position + 6, end_of_line-host_position-6);
        size_t  position_of_port = host.find(":",0);
        if(position_of_port == string::npos){
            parse_request_line();
            size_t position_of_port_new = request_line.find(":", 0);
            if((position_of_port_new != string::npos) &&(request_line.find("CONNECT", 0) != string::npos)){
                int end_of_port = request_line.find(" ", position_of_port_new + 1);
                port = request_line.substr(position_of_port_new + 1, end_of_port - position_of_port_new - 1);
            }
            else{
                port = "80";
            }
        }
        else{
            port =host.substr(position_of_port+1);
            host = host.substr(0, position_of_port);
        }
    }

    int find_content_length(string str){
        int content_length = -1;
        size_t content_length_position = str.find("Content-Length: ", 0);
        if(content_length_position != string::npos){
            int end_of_line = str.find("\r\n", content_length_position);
            content_length =stoi(str.substr(content_length_position + 16, end_of_line-content_length_position - 16),nullptr,10);
        }
        return content_length;
    }

    int find_header_length(string str){
        int header_length = -1;
        size_t  header_length_position = str.find("\r\n\r\n", 0);
        if(header_length_position != string::npos){
            header_length = header_length_position + 4;
        }
        return header_length;
    }
};


#endif