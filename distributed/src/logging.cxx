#include "logging.h"

//GLOBAL SERVER ID, SET NAME IN STATE CONSTRUCTOR
std::string* server_id = NULL;

void log(MSG_TYPE mtype, LOG_TYPE ltype, std::string& msg){
    if(mtype > LOG_LEVEL){
        return;
    }
    if(server_id == NULL){
        std::cout << "LOCAL(NO_ID_ASSIGNED)::";
    }
    else{
        std::cout << *server_id << "::";
    }
    
    switch(ltype){
        case(INFO): std::cout << "INFO::"; break;
        case(WARN): std::cout << "WARN::"; break;
        case(ERROR): std::cout << "ERROR::"; break;
        default: std::cout << "INFO::";
    }
    std::cout << msg << std::endl;
}

void log_info(const std::string& msg){
    if(LOG_INFO != 1){
        return;
    }
    if(server_id == NULL){
        std::cout << "LOCAL(NO_ID_YET)::";
    }
    else{
        std::cout << *server_id << "::";
    }
    std::cout << "INFO::" << msg << std::endl;
}

void log_error(const std::string& msg){
    if(server_id == NULL){
        std::cout << "LOCAL(NO_ID_YET)::";
    }
    else{
        std::cout << *server_id << "::";
    }
    std::cout << "ERROR::" << msg << std::endl;
}

void log_network(const std::string& msg){
    if(server_id == NULL){
        std::cout << "LOCAL(NO_ID_YET)::";
    }
    else{
        std::cout << *server_id << "::";
    }
    std::cout << "NETWORK::" << msg << std::endl;
}

void log_send(const std::string& ip, uint16_t port, const std::string& msg){
    if(LOG_NETWORK != 1){
        return;
    }
    if(server_id == NULL){
        std::cout << "LOCAL(NO_ID_YET) ";
    }
    else{
        std::cout << *server_id << " ";
    }

    std::cout << ip << ":" << port << " send::" << msg << std::endl;
    
}

void log_recv(const std::string& ip, uint16_t port, const std::string& msg){
    if(LOG_NETWORK != 1){
        return;
    }
    if(server_id == NULL){
        std::cout << "LOCAL(NO_ID_YET) ";
    }
    else{
        std::cout << *server_id << " ";
    }

    std::cout << ip << ":" << port << " recv::" << msg << std::endl;
    
}

void log_recv(struct sockaddr_in* cur_client, const std::string& msg){
    if(LOG_NETWORK != 1){
        return;
    }
    if(server_id == NULL){
        std::cout << "LOCAL(NO_ID_YET) ";
    }
    else{
        std::cout << *server_id << " ";
    }

    char* ip = inet_ntoa(cur_client->sin_addr);
    uint16_t port = ntohs(cur_client->sin_port);

    std::cout << ip << ":" << port << " recv::" << msg << std::endl;
}
