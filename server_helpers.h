#ifndef SERVER_HELPER_H
#define SERVER_HELPER_H

#include "proto.h"
#include "state.h"
#include "send.h"

#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>



//needed structures:
//user<->ip/port
//user<->channels+active channel
//channel<->users

void handle_login(struct state* data, char* buffer, size_t len, struct sockaddr_in* client){
	struct request_login* request = (struct request_login*) buffer;

	request->req_username[USERNAME_MAX - 1] = '\0';

	char ip_port[22];
	char* client_ip = inet_ntoa(client->sin_addr); //ntoa null terminated
	//printf("Got client_ip %s\n", client_ip);
	construct_ip_port_key(client_ip, ntohs(client->sin_port), ip_port);

	idempotent_add_user(data, request->req_username, ip_port, client, len);
	printf("INFO: user %s logged in\n", request->req_username);
}


void handle_logout(struct state* data, char* buffer, size_t len, struct sockaddr_in* client){
	struct request_logout* request = (struct request_logout*) buffer;
	if(request->req_type != REQ_LOGOUT){
		printf("ERROR: Invalid logout request (%lu bytes)\n", len);
		return;
	}

	char ip_port[22];
	char* client_ip = inet_ntoa(client->sin_addr); //ntoa null terminated
	construct_ip_port_key(client_ip, ntohs(client->sin_port), ip_port);

	idempotent_remove_user(data, NULL, ip_port);
	printf("INFO: client at %s logged out\n", ip_port);

}

void handle_request_join(struct state* data, char* buffer, size_t len, struct sockaddr_in* client){
	struct request_join* request = (struct request_join*) buffer;

	request->req_channel[CHANNEL_MAX - 1] = '\0';

	char ip_port[22];
	char* client_ip = inet_ntoa(client->sin_addr); //ntoa null terminated
	construct_ip_port_key(client_ip, ntohs(client->sin_port), ip_port);

	struct user* user = get_user(data, NULL, ip_port);
	if(user == NULL){
		send_error(data->sockfd, "User not currently logged in", client, len);
		printf("WARN: User not logged in\n");
		long count;
		struct user** users = list_users(data, &count);
		printf("Currently logged in users:\n");
		for(long i = 0; i < count; i++){
			printf("%s (%s)\n", users[i]->username, users[i]->ip_port);
		}
		free(users);
		return;
	}

	update_last_seen(data, user->username, NULL, 0);

	idempotent_channel_add_user(data, request->req_channel, user->username);
	printf("INFO: user %s joined channel %s\n", user->username, request->req_channel);
}

void handle_request_leave(struct state* data, char* buffer, size_t len, struct sockaddr_in* client){
	struct request_leave* request = (struct request_leave*) buffer;

	request->req_channel[CHANNEL_MAX - 1] = '\0';

	char ip_port[22];
	char* client_ip = inet_ntoa(client->sin_addr); //ntoa null terminated
	construct_ip_port_key(client_ip, ntohs(client->sin_port), ip_port);

	struct user* user = get_user(data, NULL, ip_port);
	if(user == NULL){
		send_error(data->sockfd, "User not currently logged in", client, len);
		printf("WARN: User not found\n");
		long count;
		struct user** users = list_users(data, &count);
		printf("Currently logged in users:\n");
		for(long i = 0; i < count; i++){
			printf("%s\n", users[i]->username);
		}
		free(users);
		return;
	}

	update_last_seen(data, user->username, NULL, 0);
	printf("INFO: user %s left channel %s\n", user->username, request->req_channel);

	idempotent_channel_remove_user(data, request->req_channel, user->username);
	idempotent_remove_channel_if_empty(data, request->req_channel);

	
}

void handle_request_say(struct state* data, char* buffer, size_t len, struct sockaddr_in* client){
	struct request_say* request = (struct request_say*) buffer;

	request->req_channel[CHANNEL_MAX - 1] = '\0';
	request->req_text[SAY_MAX - 1] = '\0';

	char ip_port[22];
	char* client_ip = inet_ntoa(client->sin_addr); //ntoa null terminated
	construct_ip_port_key(client_ip, ntohs(client->sin_port), ip_port);

	struct user* user = get_user(data, NULL, ip_port);
	if(user == NULL){
		send_error(data->sockfd, "User not currently logged in", client, len);
		return;
	}
	update_last_seen(data, user->username, NULL, 0);

	struct channel_users_list* channel = get_channel(data, request->req_channel);
	if(channel == NULL){
		send_error(data->sockfd, "Channel does not exist", client, len);
		return;
	}

	if(channel_contains_user(channel, user->username) == 0){
		send_error(data->sockfd, "User is not part of channel", client, len);
		return;
	}

	long entries = -1;
	char** user_arr = channel_get_user_list(channel, &entries);

	if(user_arr == NULL){
		send_error(data->sockfd, "Error sending message", client, len);
		return;
	}

	for(long i = 0; i < entries; i++){
		struct user* cur_usr = get_user(data, user_arr[i], NULL);
		if(cur_usr != NULL){
			send_message(data->sockfd, request->req_text, request->req_channel, user->username, cur_usr->cliaddr, cur_usr->len);
		}
		
	}

	printf("INFO: User %s sent message to channel %s: %s\n", user->username, request->req_channel, request->req_text);
	free(user_arr);
}

void handle_request_list(struct state* data, char* buffer, size_t len, struct sockaddr_in* client){
	struct request_list* request = (struct request_list*) buffer;
	if(request->req_type != REQ_LIST){
		printf("ERROR: Invalid list request (%lu bytes)\n", len);
		return;
	}
	update_last_seen(data, NULL, client, len);
	long count = 0;
	char** channels = get_channel_list(data, &count);
	if(channels == NULL){
		send_error(data->sockfd, "Error getting channels", client, len);
		return;
	}

	printf("INFO: channel list was requested\n");
	send_channel_list(data->sockfd, channels, count, client, len);
	free(channels);
}

void handle_request_who(struct state* data, char* buffer, size_t len, struct sockaddr_in* client){
	update_last_seen(data, NULL, client, len);
	struct request_who* request = (struct request_who*) buffer;

	request->req_channel[CHANNEL_MAX - 1] = '\0';

	struct channel_users_list* channel = get_channel(data, request->req_channel);
	if(channel == NULL){
		send_error(data->sockfd, "Channel does not exist", client, len);
		return;
	}

	long size = 0;
	char** users = channel_get_user_list(channel, &size);

	if(users == NULL){
		send_error(data->sockfd, "Internal server error while getting users in channel", client, len);
		return;
	}

	send_user_list(data->sockfd, users, size, channel->channel_name, client, len);
	free(users);
	printf("INFO: List of users in channel %s was requested\n", request->req_channel);
}

void handle_request_keep_alive(struct state* data, char* buffer, size_t len, struct sockaddr_in* client){
	struct request_keep_alive* request = (struct request_keep_alive*) buffer;
	if(request->req_type != REQ_KEEP_ALIVE){
		printf("ERROR: invalid keep alive request (%lu bytes)\n", len);
		return;
	}
	update_last_seen(data, NULL, client, len);
}

void bubble_up_error(struct state* data, char* err_msg, size_t len, struct sockaddr_in* client){
	send_error(data->sockfd, err_msg, client, len);
}

void scan_and_remove_inactive_users(struct state* data){
	struct user** users;
	long count;
	users = list_users(data, &count);

	for(long i = 0; i < count; i++){
		struct user* cur_user = users[i];
		if(time(NULL) - cur_user->last_seen > 120){
			printf("INFO: Removing user %s (%s) for inactivity (inactive for: %lu seconds)\n", cur_user->username, cur_user->ip_port, time(NULL) - cur_user->last_seen);
			idempotent_remove_user(data, cur_user->username, NULL);
		}
	}
	free(users);
}
#endif