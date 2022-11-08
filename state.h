#ifndef STATE_H
#define STATE_H
//STATE.H IS MANAGES THE STATE AND PROVIDES

#include "proto.h"
#include "adts/linkedlist.h"
#include "adts/hashmap.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>


#define IP_PORT_SIZE 22
/*
We create a central linkedlist of all the users
We create two hashmaps
The first has IP/port as the key and points to an entry in the central linkedlist
The second has username as a key and points to an entry in the central linkedlist
This is sort of like having a static SQL JOIN statement between a user table and an IP table
*/

struct user {
	char username[USERNAME_MAX];
	char ip_port[IP_PORT_SIZE];
	struct sockaddr_in* cliaddr;
	size_t len;
	unsigned int last_seen;
};

struct channel_users_list {
	char channel_name[CHANNEL_MAX];
	const LinkedList* user_list;
};

struct state {
	int sockfd;
	const LinkedList* user_list; //this is the list of logged in users
	const HashMap* user_by_ip;
	const HashMap* user_by_username;
	const HashMap* channel_user_lists; //this one has lists of logged out users that are still subscribed to a channel
	pthread_mutex_t lock;
	pthread_t cleanup_thread;
};

struct state* alloc_state(int sockfd);
void free_state(struct state* data);

void construct_ip_port_key(char* ip, uint16_t port, char* dest);
void extract_ip_from_combo(char* ip_port, char* dest);
uint16_t extract_port_from_combo(char* ip_port);

void idempotent_remove_user(struct state* data, char* username, char* ip_port);
void idempotent_add_user(struct state* data, char* username, char* ip_port, struct sockaddr_in* cliaddr, size_t len);
struct user* get_user(struct state* data, char* username, char* ip_port);
void update_last_seen(struct state* data, char* username, struct sockaddr_in* cliaddr, size_t len);
struct user** list_users(struct state* data, long* count);

void idempotent_remove_channel(struct state* data, char* channel_name);
void idempotent_add_channel(struct state* data, char* channel_name);
void idempotent_remove_channel_if_empty(struct state* data, char* channel_name);
char** get_channel_list(struct state* data, long* count);

int get_channel_user_count(struct state* data, char* channel_name);
void idempotent_channel_add_user(struct state* data, char* channel_name, char* username);
void idempotent_channel_remove_user(struct state* data, char* channel_name, char* username);
struct channel_users_list* get_channel(struct state* data, char* channel_name);
int channel_contains_user(struct channel_users_list* channel, char* username);
char** channel_get_user_list(struct channel_users_list* channel, long* len);

#endif