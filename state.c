#include "state.h"

#define LOG_STATE_LEVEL 1

void logsl(char* fmt, char* str){
	if(LOG_STATE_LEVEL == 1){
		printf("INFO::STATE_LEVEL::");
		if(str == NULL){
			printf("%s\n", fmt);
			return;
		}
		printf(fmt, str);
	}
}

struct state* alloc_state(int sockfd){
	struct state* data = malloc(sizeof(struct state));
	data->user_list = LinkedList_create();
	data->user_by_ip = HashMap_create(32L, 0.5f);
	data->user_by_username = HashMap_create(32L, 0.5f);
	data->channel_user_lists = HashMap_create(32L, 0.5f);
	data->sockfd = sockfd;
	return data;
}



void free_user_f(void* e){
	if(e != NULL){
		struct user* user = (struct user*) e;
		free(user->cliaddr);
		free(user);
	}
}
void (*free_user)(void*) = &free_user_f;


void free_channel_f(void* e){
	if(e != NULL){
		struct channel_users_list* channel = (struct channel_users_list*) e;
		channel->user_list->destroy(channel->user_list, free);
		free(channel);
	}
}
void (*free_channel)(void*) = &free_channel_f;

void free_state(struct state* data){
	data->user_list->destroy(data->user_list, free_user);
	data->user_by_ip->destroy(data->user_by_ip, NULL);
	data->user_by_username->destroy(data->user_by_username, NULL);
	data->channel_user_lists->destroy(data->channel_user_lists, free_channel);
}


int user_compare_f(void* e1, void* e2){
	struct user* u_e1 = (struct user*) e1;
	struct user* u_e2 = (struct user*) e2;
	int uname_comp = strcmp(u_e1->username, u_e2->username);
	int ip_comp = strcmp(u_e1->ip_port, u_e2->ip_port);
	if(uname_comp == 0 && ip_comp == 0){
		return 1;
	}
	return 0;
}
int (*user_compare)(void*, void*) = &user_compare_f;


int char_compare_f(void* e1, void* e2){
	char* c_e1 = (char*) e1;
	char* c_e2 = (char*) e2;
	if(strcmp(c_e1, c_e2) == 0){
		return 1;
	}
	return 0;
}
int (*char_compare)(void*, void*) = &char_compare_f;

//compare must return 1 if same 0 otherwise
//returns -1 if not found
int ll_index_of(const LinkedList* ll, void* target_element, int (*compare)(void* e1, void* e2)){
	void* cur_element;
	for(int i = 0; i < ll->size(ll); i++){
		ll->get(ll, i, (void**) &cur_element);
		if((*compare)(cur_element, target_element) == 1){
			return i;
		}
	}

	return -1;
}

//no guarentees about where in the LL it will appear, just that it will appear
void ll_idempotent_add(const LinkedList* ll, void* entry, int (*compare)(void* e1, void* e2)){
	if(ll_index_of(ll, entry, compare) != -1){
		return;
	}

	ll->add(ll, entry);
}

void ll_idempotent_remove(const LinkedList* ll, void* entry, int (*compare)(void* e1, void* e2), void (*freeFxn)(void* e)){
	int idx = ll_index_of(ll, entry, compare);
	if(idx != -1){
		logsl("ll_idempotent_remove found entry %s\n", (char*) entry);
		void* temp;
		int status = ll->remove(ll, idx, &temp);
		if(status == 1){
			(*freeFxn)(temp);
		}
		else{
			logsl("ERROR: ll_idempotent_remove found entry but failed to remove it\n", NULL);
		}
		
	}
}

//dest must be 22 chars
void construct_ip_port_key(char* ip, uint16_t port, char* dest){
	if(dest == NULL){
		printf("construct_ip_port_key invoked with NULL dest\n");
	}
	char* combo = dest;
	memset(combo, '\0', IP_PORT_SIZE);
	strncpy(combo, ip, 15);
	strcat(dest, ":");

	char port_chars[6];
	memset(port_chars, '\0', 6);
	sprintf(port_chars, "%u", ntohs(port));

	strcat(dest, port_chars);
	combo[21] = '\0';
	//printf("Got IP %s and PORT %u, made [%s]\n", ip, port, combo);
}

//dest must be 16 bytes
void extract_ip_from_combo(char* ip_port, char* dest){
	memset(dest, '\0', 16);
	size_t i = 0;
	while(ip_port[i] != ':'){
		dest[i] = ip_port[i];
		i++;
	}
}

uint16_t extract_port_from_combo(char* ip_port){
	char buf[6];
	size_t i = 0;
	size_t buf_idx = 0;
	while(ip_port[i] != ':'){
		i++;
	}
	i++;
	while(ip_port[i] != '\0'){
		buf[buf_idx] = ip_port[i];
		i++;
		buf_idx++;
	}
	return (uint16_t) atoi(buf);
}

//fill either username or ip_port and set the other null
//if both are provided, will use username
void idempotent_remove_user(struct state* data, char* username, char* ip_port){
	if(username != NULL){
		struct user* user;
		int got_user = data->user_by_username->remove(data->user_by_username, username, (void**) &user);
		logsl("removing user by username\n", NULL);
		if(got_user){
			logsl("found user %s\n", username);
			struct user* old;
			data->user_by_ip->remove(data->user_by_ip, user->ip_port, (void**) &old);
			ll_idempotent_remove(data->user_list, (void*) user, user_compare, free_user);
		}
		else{
			logsl("did not find user %s\n", username);
		}
		return;
	}

	if(ip_port != NULL){
		struct user* user;
		int got_user = data->user_by_ip->remove(data->user_by_ip, ip_port, (void**) &user);
		logsl("removing user by ip_port\n", NULL);
		if(got_user){
			logsl("found user %s\n", ip_port);
			struct user* old;
			data->user_by_username->remove(data->user_by_username, user->username, (void**) &old);
			ll_idempotent_remove(data->user_list, (void*) user, user_compare, free_user);
		}
		else{
			logsl("did not find user %s\n", ip_port);
		}
		return;
	}

	printf("ERROR: idempotent_remove_user called with neither username nor ip_port provided\n");

}

void idempotent_add_user(struct state* data, char* username, char* ip_port, struct sockaddr_in* cliaddr, size_t len){
	//if the user is already logged in (potentially from another ip/port), log them out
	//if the machine is already logged in (potentially as another user), log them out
	idempotent_remove_user(data, username, NULL);
	idempotent_remove_user(data, NULL, ip_port);

	struct user* user = malloc(sizeof(struct user));
	memcpy(user->username, username, USERNAME_MAX);
	memcpy(user->ip_port, ip_port, IP_PORT_SIZE);
	user->last_seen = time(NULL);
	user->cliaddr = malloc(sizeof(char) * len);
	user->len = len;
	memcpy(user->cliaddr, cliaddr, len);


	ll_idempotent_add(data->user_list, user, user_compare);
	data->user_by_ip->putUnique(data->user_by_ip, ip_port, (void*) user);
	data->user_by_username->putUnique(data->user_by_username, username, (void*) user);
}

struct user** list_users(struct state* data, long* count){
	logsl("listing users\n", NULL);
	return (struct user**) data->user_list->toArray(data->user_list, count);
}

void idempotent_remove_channel(struct state* data, char* channel_name){
	struct channel_users_list* channel;
	int status = data->channel_user_lists->remove(data->channel_user_lists, channel_name, (void**) &channel);
	if(status == 0){
		return;
	}
	const LinkedList* ll = channel->user_list;
	ll->destroy(ll, free);
	free(channel);
}

void idempotent_add_channel(struct state* data, char* channel_name){
	int exists = data->channel_user_lists->containsKey(data->channel_user_lists, channel_name);
	if(exists == 1){
		return;
	}

	const LinkedList* ll = LinkedList_create();
	struct channel_users_list* channel = malloc(sizeof(struct channel_users_list));

	memcpy(channel->channel_name, channel_name, CHANNEL_MAX);
	channel->user_list = ll;

	data->channel_user_lists->putUnique(data->channel_user_lists, channel_name, channel);
}

void idempotent_channel_add_user(struct state* data, char* channel_name, char* username){
	idempotent_add_channel(data, channel_name);

	struct channel_users_list* channel;
	data->channel_user_lists->get(data->channel_user_lists, channel_name, (void**) &channel);

	char* username_dup = strdup(username); //not having this caused a bug where someone leaving a channel would deallocate their name in the main  user list
	ll_idempotent_add(channel->user_list, (void*) username_dup, char_compare);
}

void idempotent_channel_remove_user(struct state* data, char* channel_name, char* username){
	int exists = data->channel_user_lists->containsKey(data->channel_user_lists, channel_name);
	if(exists == 0){
		return;
	}

	struct channel_users_list* channel;
	data->channel_user_lists->get(data->channel_user_lists, channel_name, (void**) &channel);

	ll_idempotent_remove(channel->user_list, (void*) username, char_compare, free);
}

int get_channel_user_count(struct state* data, char* channel_name){
	struct channel_users_list* channel;

	int status = data->channel_user_lists->get(data->channel_user_lists, channel_name, (void**) &channel);
	if(status == 0){
		return -1;
	}

	return channel->user_list->size(channel->user_list);
}

void idempotent_remove_channel_if_empty(struct state* data, char* channel_name){
	int user_count = get_channel_user_count(data, channel_name);
	if(user_count == -1){
		return;
	}
	if(user_count == 0){
		idempotent_remove_channel(data, channel_name);
	}
}

struct user* get_user(struct state* data, char* username, char* ip_port){
	struct user* user;
	int status = 0;
	if(username != NULL){
		status = data->user_by_username->get(data->user_by_username, username, (void**) &user);
		if(status == 0){
			return NULL;
		}
		return user;
	}

	if(ip_port != NULL){
		status = data->user_by_ip->get(data->user_by_ip, ip_port, (void**) &user);
		if(status == 0){
			return NULL;
		}
		return user;
	}
	return NULL;
}

struct channel_users_list* get_channel(struct state* data, char* channel_name){
	struct channel_users_list* channel = NULL;
	int status = data->channel_user_lists->get(data->channel_user_lists, channel_name, (void**) &channel);
	if(status == 0){
		return NULL;
	}
	return channel;
}

int channel_contains_user(struct channel_users_list* channel, char* username){
	int idx = ll_index_of(channel->user_list, (void*) username, char_compare);
	if(idx == -1){
		return 0;
	}
	return 1;
}

char** channel_get_user_list(struct channel_users_list* channel, long* len){
	return (char**) channel->user_list->toArray(channel->user_list, len);
}

char** get_channel_list(struct state* data, long* count){
	return data->channel_user_lists->keyArray(data->channel_user_lists, count);
}

void update_last_seen(struct state* data, char* username, struct sockaddr_in* cliaddr, size_t len){
	char ip_port[22];
	if(username == NULL){
		if(cliaddr == NULL){
			printf("No username or cliaddr provided to update_last_seen\n");
			return;
		}
		char* client_ip = inet_ntoa(cliaddr->sin_addr); //ntoa null terminated
		construct_ip_port_key(client_ip, ntohs(cliaddr->sin_port), ip_port);
	}
	struct user* user = get_user(data, username, ip_port);
	if(user == NULL){
		if(username != NULL){
			printf("ERROR: User %s cannot be found to update last seen\n", username);
			return;
		}
		if(cliaddr != NULL){
			printf("ERROR: User at %s cannot be found to update last seen (%lu bytes)\n", ip_port, len);
			return;
		}
		printf("ERROR: Neither username nor ip_port specified\n");
		return;
	}
	user->last_seen = time(NULL);

}