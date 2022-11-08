#ifndef LOGGING_H
#define LOGGING_H
#include <iostream>
#include <string>
#include <sstream>

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//0 is normal, 1 is on net debug only, 2 is all debug
#define LOG_LEVEL ALL

enum MSG_TYPE {
    NORMAL = 0,
    NET = 1,
    ALL = 2
};

enum LOG_TYPE {
    ERROR,
    WARN,
    INFO
};

#define LOG_INFO 0
#define LOG_ERROR 1
#define LOG_NETWORK 1

//GLOBAL SERVER ID, SET NAME IN STATE CONSTRUCTOR
extern std::string* server_id;

void log(MSG_TYPE mtype, LOG_TYPE ltype, std::string& msg);
void log_info(const std::string& msg);
void log_error(const std::string& msg);
void log_network(const std::string& msg);
void log_send(const std::string& ip, uint16_t port, const std::string& msg);
void log_recv(const std::string& ip, uint16_t port, const std::string& msg);
void log_recv(struct sockaddr_in* cur_client, const std::string& msg);
#endif