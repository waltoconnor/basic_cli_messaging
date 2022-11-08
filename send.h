#ifndef SEND_H
#define SEND_H
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 

#include "proto.h"

void send_msg(int sockfd, struct request* req, size_t req_size, struct sockaddr_in* cliaddr, size_t len){
	printf("INFO: Sending\n");
	//int n = sendto(sockfd, (const void*) req, req_size, MSG_CONFIRM, (const struct sockaddr*) cliaddr, len);
	int n = sendto(sockfd, (const void*) req, req_size, 0, (const struct sockaddr*) cliaddr, len);
	printf("INFO: sendto returned %d bytes sent\n", n);
	if(n < 0){
		printf("WARN: sendto VALUE IS ERROR\n");
	}
}

void send_error(int sockfd, char* text, struct sockaddr_in* cliaddr, size_t len){
	struct text_error* msg = malloc(sizeof(struct text_error));
	size_t size = sizeof(struct text_error);
	size_t text_len = strlen(text);
	msg->txt_type = TXT_ERROR;
	if(text_len < SAY_MAX){
		memset(msg->txt_error, '\0', SAY_MAX);
		memcpy(msg->txt_error, text, text_len);
		send_msg(sockfd, (struct request*) msg, size, cliaddr, len);
	}

	free(msg);
}


void send_message(int sockfd, char* text, char* channel, char* username, struct sockaddr_in* cliaddr, size_t len){
	struct text_say* msg = malloc(sizeof(struct text_say));
	size_t size = sizeof(struct text_say);

	msg->txt_type = TXT_SAY;

	memset(msg->txt_channel, '\0', CHANNEL_MAX);
	memset(msg->txt_username, '\0', USERNAME_MAX);
	memset(msg->txt_text, '\0', SAY_MAX);

	memcpy(msg->txt_channel, channel, strlen(channel));
	memcpy(msg->txt_username, username, strlen(username));
	memcpy(msg->txt_text, text, strlen(text));

	send_msg(sockfd, (struct request*) msg, size, cliaddr, len);
	free(msg);
}

void send_channel_list(int sockfd, char** channels, long count, struct sockaddr_in* cliaddr, size_t len){
	size_t size = sizeof(struct text_list) + (sizeof(struct channel_info) * count);
	struct text_list* list = malloc(size);
	list->txt_type = TXT_LIST;
	list->txt_nchannels = (int) count;

	for(long i = 0; i < count; i++){
		memset(list->txt_channels[i].ch_channel, '\0', CHANNEL_MAX);
		memcpy(list->txt_channels[i].ch_channel, channels[i], strlen(channels[i]));
	}

	send_msg(sockfd, (struct request*) list, size, cliaddr, len);
	free(list);

}

void send_user_list(int sockfd, char** users, long count, char* channel, struct sockaddr_in* cliaddr, size_t len){
	size_t size = sizeof(struct text_who) + (sizeof(struct user_info) * count);
	struct text_who* who = malloc(size);
	who->txt_type = TXT_WHO;
	
	memset(who->txt_channel, '\0', CHANNEL_MAX);
	memcpy(who->txt_channel, channel, strlen(channel));

	who->txt_nusernames = (int) count;

	for(long i = 0; i < count; i++){
		memset(who->txt_users[i].us_username, '\0', USERNAME_MAX);
		memcpy(who->txt_users[i].us_username, users[i], strlen(users[i]));
	}

	send_msg(sockfd, (struct request*) who, size, cliaddr, len);
	free(who);
}

#endif