#ifndef __CACHE__H__
#define __CACHE__H__
#include <time.h>
#include <string>
#include <unordered_map>
#include "http_struct.h"
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
#include <mutex>

mutex mtx1,mtx2,mtx3,mtx4;
class Cache{
public:
    unordered_map<string, string> request_cache;
public:
    size_t parse_Cacheability_no_store(string str){
        return (str.find("no-store", 0));
    }

    size_t parse_Cacheability_no_cache(string str){
        return (str.find("no-cache", 0));
    }

    size_t parse_Cacheability_revalidation(string str){
        return (str.find("must-revalidate", 0));
    }

    size_t parse_Cacheability_cache_control(string str){
        return (str.find("Cache-Control: ", 0));
    }
    
    int parse_Cacheability_max_age(string str){
        int max_age = -1;
        size_t max_age_position = str.find("max-age=", 0);
        size_t end_of_line = str.find("\r\n", max_age_position + 1);
        if(max_age_position != string::npos){
            max_age = stoi(str.substr(max_age_position + 8, end_of_line-max_age_position-8), nullptr, 10);
        }
        return max_age;
    }

    time_t parse_Cacheability_expire(string str){

        size_t expire_position = str.find("Expires: ", 0);
        size_t end_of_line = str.find("\r\n", expire_position + 1);
        string expire = str.substr(expire_position + 9, end_of_line - expire_position - 9);
        struct tm e;
        strptime(expire.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &e);
        time_t expire_time = mktime(&e);
        return expire_time;
    }

    time_t parse_Cacheability_date(string str){
         size_t date_position = str.find("Date: ", 0);
         size_t end_of_line = str.find("\r\n", date_position + 1);
         string date = str.substr(date_position + 6, end_of_line - date_position - 6);
         struct tm e;
         strptime(date.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &e);
         time_t date_time = mktime(&e);
         return date_time;
    }
    
    string parse_Cacheability_Etags(string str){
        string etag;
        size_t etag_position = str.find("ETag: ", 0);
        if(etag_position != string::npos){
            size_t end_of_line = str.find("\r\n", etag_position + 1);
            etag = str.substr(etag_position + 6, end_of_line - etag_position - 6);
        }
        return etag;
    }

    string parse_Cacheability_Last_Modified(string str){
        string last_modified;
        size_t last_modified_position = str.find("Last-Modified: ", 0);
        if(last_modified_position != string::npos){
            size_t end_of_line = str.find("\r\n", last_modified_position + 1);
            last_modified = str.substr(last_modified_position + 15, end_of_line - last_modified_position - 15);
        }
        return last_modified;
    }

    bool parse_ok(string str){
        if(str.find("200 OK") != string::npos){
            return true;
        }
        return false;
    }

    /*update cache properly***/
    void update_cache(string request, string response, Http_struct http_struct, int thread_id, ofstream & file){
        mtx1.lock();
        if(response.find("no-store", 0) != string::npos){
            file << thread_id << ": NOTE Response_cache-control: no-store" << endl;
            return;
        }
        if(request_cache.size() == 50){
            //cout << request_cache.size() << endl;
            file <<"(no-id): NOTE delete " << http_struct.URL << " from cache" <<endl;
            request_cache.erase(request_cache.begin());
        }
        file << thread_id << ": request-response cached successfully" << endl;
        if(response.find("Expires: ", 0) != string::npos){
            time_t expire_time = parse_Cacheability_expire(response);
            file << thread_id <<": Cached, expires at: " <<asctime(gmtime(&expire_time));
        }
        if((response.find("no-cache", 0) != string::npos) || (response.find("must-revalidate", 0)) != string::npos){
            file << thread_id << ": Cached, but requires re-validation"<< endl;
        }
        request_cache[request] = response;
        mtx1.unlock();
    }

    /*search through cache and return a cached or revalidated response****/
    string search_cache(string request, Http_struct http_struct, int socket_fd, int thread_id, ofstream & file){
        string actual_response;
        mtx3.lock();
        unordered_map<string, string>::iterator hit = request_cache.find(request);
        unordered_map<string, string>::iterator end = request_cache.end();
        mtx3.unlock();
        if(hit != end){
            //in cache
            file << thread_id << ": in cache" << endl;
            mtx4.lock();
            string response_cache = request_cache[request];
            mtx4.unlock();
            actual_response = response_cache;
            if(parse_Cacheability_no_cache(response_cache) != string::npos){
                file<<thread_id<<": NOTE Response-cache-control: no-cache " << endl;
                file<<thread_id<<": NOTE need revalidation" << endl;
                actual_response = revalidate(request, socket_fd, response_cache, http_struct,thread_id, file);//send revalidate request to server
            }
            else{
                int max_age = -1;
                if((max_age = parse_Cacheability_max_age(response_cache)) != -1){
                    file << thread_id <<": NOTE Cache-Control: max-age= " << max_age << endl;
                    time_t currtime = time(NULL);
                    time_t date = parse_Cacheability_date(response_cache);
                    double seconds = difftime(currtime, date - timezone);
                    /*cout << "-----------" << endl;
                    cout <<"***********" << endl;
                    cout << seconds << endl;
                    cout << max_age << endl;
                    cout << "*********" << endl;
                    cout << "-----------" << endl;*/
                    if(seconds >= max_age){
                        //espires
                        file << thread_id << ": NOTE Cached response expires" <<endl;
                        if(parse_Cacheability_revalidation(response_cache) != string::npos){
                            //must-revalidate
                            file << thread_id <<": NOTE Cache-Control: must-revalidation" <<endl;
                            actual_response = revalidate(request, socket_fd, response_cache, http_struct, thread_id, file);//need to revalidate
                            return actual_response;
                        }
                        else{
                            file << thread_id << ": NOTE no need for validation, keep sending cached copy" << endl;
                            //no need to revalidate
                            actual_response = response_cache;
                        }
                    }
                    else{
                        file << thread_id <<": NOTE resource fresh, sending cached copy" << endl;
                        //fresh, send response_cache back to browser
                        actual_response = response_cache;
                        return actual_response;
                    }
                }
                else{
                    //no max-age
                    if(response_cache.find("Expires: ", 0) != string::npos){
                        time_t currtime;
                        time(&currtime);
                        time_t expire_time = parse_Cacheability_expire(response_cache);
                        double seconds = difftime(currtime, expire_time - timezone);
                        file << thread_id <<": NOTE Expires at: " <<asctime(gmtime(&expire_time));
                        if(seconds>= 0){
                            //expires
                            if(parse_Cacheability_revalidation(response_cache) != string::npos){
                                //must-revalidate
                                file<<thread_id<<": NOTE Cache-Control: must-revalidation" <<endl;
                                actual_response = revalidate(request, socket_fd, response_cache, http_struct, thread_id, file);//need to revalidate
                                return actual_response;
                            }
                            else{
                                //no need to revalidate
                                file<<thread_id<< ": NOTE no need for validation, keep sending cached copy" << endl;
                                actual_response = response_cache;
                            }
                        }
                        else{
                            //fresh, send response_cache back to browser
                            file<<thread_id<<": NOTE resource fresh, sending cached copy" << endl;
                            actual_response = response_cache;
                            return actual_response;
                        }
                    }//if expires find
                }//no max-age
            }//no no-cache
        }//in cache
        else{
            //not in cache
        }
        return actual_response;
    }

    /*send conditional request to server to revalidate response
    *see if response with 304 or 200
    *and return a cached or revalidated response accordingly*****/
    string get_validation_response(Http_struct http_struct, string request, int socket_fd, string response_cache, int thread_id, ofstream & file){
        int numbytes;
        string response;
        if((numbytes = send(socket_fd, request.c_str(), request.length(), 0)) == -1){
            perror("Send: ");
        }
        //cout <<"Bytes send to server "<< numbytes << endl;
        int message_length;
        int buffersize = 100;
        std::vector<char> buffer(buffersize);
        do{
            char *p = buffer.data();
            if((message_length = recv(socket_fd, p, buffersize, 0)) == -1){
                perror("receive error");
            }
            string temp(buffer.begin(), buffer.begin() + message_length);
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
                    break;
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
        /* cout << endl;
        cout <<"--------------"<<endl;
        cout <<thread_id << endl;
        cout <<response << endl;
        cout <<"--------------"<<endl;
        cout << endl;*/
        size_t status, response_line_position;
        string response_line;
        if((status = response.find("304 Not Modified" ,0)) != string::npos){
            file << thread_id << ": NOTE cache copied vaild, send copy" << endl;
            response = response_cache;
        }
        else{
            if(response.find("200 OK") != string::npos){
                update_cache(request, response, http_struct, thread_id, file);
            }
            if((response_line_position = response.find("\r\n", 0)) != string::npos){
                response_line = response.substr(0, response_line_position);
                file << thread_id << ": Received \"" << response_line << "\" from " << http_struct.host<< endl;
            }
        }
        return response;
    }

    /*add etag and last-modified info into request's header field
    *to build a conditional request
    *then call revalidate_response to go to server and revalidate***/
    string revalidate(string request, int server_sock_fd, string response_cache, Http_struct http_struct, int thread_id, ofstream & file){
        string revalidate_response;
        string last_modified = parse_Cacheability_Last_Modified(response_cache);
        string etag = parse_Cacheability_Etags(response_cache);
        if(!last_modified.empty()){
            string if_modified_since = "If-Modified-Since: ";
            if_modified_since.append(last_modified);
            if_modified_since.append("\r\n");
            size_t insert_pos = request.find("\r\n\r\n", 0);
            request.insert(insert_pos + 2, if_modified_since);
        }
        if(!etag.empty()){
            string if_none_match = "If-None-Match: ";
            if_none_match.append(etag);
            if_none_match.append("\r\n");
            size_t insert_pos = request.find("\r\n\r\n", 0);
            request.insert(insert_pos + 2, if_none_match);
        }
        revalidate_response = get_validation_response(http_struct, request, server_sock_fd, response_cache, thread_id, file);
        return revalidate_response;
    }
};

#endif
