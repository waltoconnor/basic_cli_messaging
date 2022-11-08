#ifndef HANDLERS_H
#define HANDLERS_H
#include <random>

#include "network.h"
#include "proto.h"
#include "state.h"
#include "logging.h"
//void name(State* data, char* buffer, size_t len, struct sockaddr_in* cur_client, size_t cli_len)



unsigned long gen_uid();

void channel_pruning_procedure(State* data, const std::vector<Channel*>* channels);

void handle_login(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
void handle_logout(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
void handle_join(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
void handle_leave(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
void handle_say(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
void handle_list(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
void handle_who(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
void handle_keep_alive(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);



void handle_s2s_join(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
void handle_s2s_say(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
void handle_s2s_leave(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
#endif