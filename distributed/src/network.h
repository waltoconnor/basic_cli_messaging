#ifndef NETWORK_H
#define NETWORK_H

#include <string>
#include <vector>

#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "logging.h"
#include "proto.h"

#define IP_LEN 16
typedef char ip_t[IP_LEN];
typedef uint32_t ip_n_t;
//port_t is always in host order
typedef uint16_t port_t;

typedef unsigned long msg_uid_t;

typedef char uname_t[USERNAME_MAX];
typedef char cname_t[CHANNEL_MAX];
typedef char say_txt_t[SAY_MAX];

class Addressable {
protected:
    port_t port;
    ip_t ip;
    std::string ip_str;
    size_t len;
    struct sockaddr_in addr;
public:
    Addressable(ip_t ip_, port_t port_){
        port = port_;
        memcpy(&ip, ip_, IP_LEN);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        
        int status = inet_aton(ip_, &(addr.sin_addr));
        if(status == 0){
            auto errstr = std::string("Unable to convert ")+std::string(ip_)+std::string(" to valid address");
            log(ALL, ERROR, errstr);
        }
        len = sizeof(struct sockaddr_in);
        ip_str = std::string(ip);
    }
    Addressable(struct sockaddr_in* addr_, size_t len_){
        memcpy(&addr, addr_, len_);
        len = len_;
        port = ntohs(addr_->sin_port);
        char* ip_buf = inet_ntoa(addr_->sin_addr);
        memcpy(&ip, ip_buf, IP_LEN);
        ip_str = std::string(ip);
    }

    std::pair<struct sockaddr_in*, size_t> get_addr(){
        return std::make_pair(&addr, len);
    }

    char* get_ip_char(){
        return ip;
    }
    const std::string* get_ip_str(){
        return &ip_str;
    }
    ip_n_t get_ip_uint(){
        in_addr ip_spot;
        int status = inet_aton(ip, &(ip_spot));
        if(status == 0){
            auto errstr = std::string("Unable to convert ")+std::string(ip)+std::string(" to valid address");
            log(ALL, ERROR, errstr);
        }
        return ip_spot.s_addr;
    }

    port_t get_port(){
        return port;
    }

    bool compare_addrs(Addressable* other){
        bool ip_match = strcmp(other->get_ip_char(), get_ip_char()) == 0;
        bool port_match = other->get_port() == get_port();
        return ip_match && port_match;
    }
};

void send_message(int sockfd, const std::vector<Addressable*>* targets, char* buffer, size_t len, const std::string& log_msg);

void broadcast_say(int sockfd, const std::vector<Addressable*>* targets, cname_t channel_name, uname_t user_name, say_txt_t msg);

void broadcast_channel_list(int sockfd, const std::vector<Addressable*>* targets, const std::vector<char*>* channels);

void broadcast_who(int sockfd, const std::vector<Addressable*>* targets, cname_t cname, const std::vector<char*>* names);

void broadcast_error(int sockfd, const std::vector<Addressable*>* targets, say_txt_t msg);


void broadcast_s2s_join(int sockfd, const std::vector<Addressable*>* targets, cname_t chan_name);

void broadcast_s2s_leave(int sockfd, const std::vector<Addressable*>* targets, cname_t chan_name);

void broadcast_s2s_say(int sockfd, const std::vector<Addressable*>* targets, msg_uid_t uid, uname_t uname, cname_t cname, say_txt_t msg);

#endif