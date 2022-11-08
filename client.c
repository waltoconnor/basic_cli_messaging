#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "raw.h"
#include "proto.h"

//lets support 1024 channels or 1024 users
//message type + len + MAX_CHANNEL*1024
//4 + 4 + 32 * 1024
#define BUFFER_MAX 32776

#define CMD_SAY 0
#define CMD_LIST 1
#define CMD_WHO 2
#define CMD_JOIN 3
#define CMD_LEAVE 4
#define CMD_EXIT 5
#define CMD_SWITCH 6


//GLOBALS
char* buffer;
pthread_t keep_alive_thread;
pthread_mutex_t lock;
int activity_last_minute = 0;

struct sockaddr_in* setup_server_info(char* hostname, char* port){
	struct hostent* host = gethostbyname(hostname);
	if(!host){
		printf("Unable to resolve hostname %s\n", hostname);
		exit(1);
	}

	struct in_addr* serv_addr_only = (struct in_addr*) host->h_addr_list[0];
	//printf("Server IP: %s\n", inet_ntoa(*serv_addr_only));

	struct sockaddr_in* final_addr = malloc(sizeof(struct sockaddr_in));
	memset(final_addr, 0, sizeof(struct sockaddr_in));

	final_addr->sin_family = AF_INET;
	final_addr->sin_addr.s_addr = serv_addr_only->s_addr;
	final_addr->sin_port = htons(atoi(port));

	return final_addr;
}

void handle_txt_say(struct request* req){
	struct text_say* say = (struct text_say*) req;
	say->txt_channel[CHANNEL_MAX - 1] = '\0';
	say->txt_username[USERNAME_MAX - 1] = '\0';
	say->txt_text[SAY_MAX - 1] = '\0';
	printf("[%s][%s]: %s\n", say->txt_channel, say->txt_username, say->txt_text);
}

void handle_txt_list(struct request* req){
	struct text_list* list = (struct text_list*) req;
	printf("Existing Channels:\n");
	for(int i = 0; i < list->txt_nchannels; i++){
		list->txt_channels[i].ch_channel[CHANNEL_MAX - 1] = '\0';
		printf(" %s\n", list->txt_channels[i].ch_channel);
	}
}

void handle_txt_who(struct request* req){
	struct text_who* who = (struct text_who*) req;
	who->txt_channel[CHANNEL_MAX - 1] = '\0';
	printf("Users on channel %s:\n", who->txt_channel);
	for(int i = 0; i < who->txt_nusernames; i++){
		who->txt_users[i].us_username[USERNAME_MAX - 1] = '\0';
		printf(" %s\n", who->txt_users[i].us_username);
	}
}

void handle_txt_error(struct request* req){
	struct text_error* err = (struct text_error*) req;
	err->txt_error[SAY_MAX - 1] = '\0';
	printf("SERVER_ERROR::%s\n", err->txt_error);
}

int check_server_message(struct request* req, int provided_len){
	switch(req->req_type){
		case TXT_SAY: return (sizeof(struct text_say) == provided_len);
			break;
		case TXT_LIST:
			return (provided_len == (4 + 4 + (CHANNEL_MAX * ((struct text_list*) req)->txt_nchannels)));
			break;
		case TXT_WHO:
			return (provided_len == (4 + 4 + CHANNEL_MAX + (USERNAME_MAX * ((struct text_who*) req)->txt_nusernames)));
			break;
		case TXT_ERROR:
			return (provided_len == (sizeof(struct text_error)));
			break;
		default: printf("Server sent unknonw txt type (type=%d)\n", req->req_type);
				 return 0;
	}

	return 0;
}
void handle_server_message(struct request* req, int provided_len){
	if(!check_server_message(req, provided_len)){
		printf("Server message (type=%d) length (%d bytes), did not match expected length\n", req->req_type, provided_len);
		return;
	}
	switch(req->req_type){
		case TXT_SAY: handle_txt_say(req);
			break;
		case TXT_LIST: handle_txt_list(req);
			break;
		case TXT_WHO: handle_txt_who(req);
			break;
		case TXT_ERROR: handle_txt_error(req);
			break;
		default: printf("Server sent unknonw txt type (type = %d)\n", req->req_type);
	}
}

void read_from_server(struct sockaddr_in* server_info, int sockfd, char* perm_buf){
	memset(perm_buf, '\0', BUFFER_MAX);
	socklen_t len;
	int n = recvfrom(sockfd, perm_buf, BUFFER_MAX, MSG_WAITALL, (struct sockaddr *) server_info, &len);
	if(n < 0){
		printf("CLIENT_ERROR::recvfrom returned status code %d\n", n);
		int recverr = errno;
		switch(recverr){
			case EBADF: printf("EBADF\n");
				break;
			case EINVAL: printf("EINVAL\n");
				break;
			case ENOTCONN: printf("ENOTCONN\n");
				break;
			default: printf("OTHER FAULT\n");
		}

	}
	else{
		handle_server_message((struct request*) perm_buf, n);
	}
}

int startswith(char* string, char* start){
	if(strlen(string) < strlen(start)){
		return 0;
	}

	char* str_alias = string;
	char* start_alias = start;

	while(*str_alias == *start_alias){
		str_alias++;
		start_alias++;
		if(*start_alias == '\0'){
			return 1;
		}
	}
	return 0;
}

//returns a pointer to the first character after the space
//or null if there is no space
char* get_arg(char* string){
	char* str = string;
	while(*str != ' '){
		if(*str == '\0'){
			return NULL;
		}
		str++;
	}
	str++;
	return str;
}

int check_if_is_command(char* cur_line){
	if(cur_line[0] != '/'){
		return CMD_SAY;
	}
	if(startswith(cur_line, "/join")){
		return CMD_JOIN;
	}
	if(startswith(cur_line, "/switch")){
		return CMD_SWITCH;
	}
	if(startswith(cur_line, "/leave")){
		return CMD_LEAVE;
	}
	if(startswith(cur_line, "/exit")){
		return CMD_EXIT;
	}
	if(startswith(cur_line, "/list")){
		return CMD_LIST;
	}
	if(startswith(cur_line, "/who")){
		return CMD_WHO;
	}
	return -1;
}

void send_msg(struct sockaddr_in* server_info, size_t len, int sockfd, struct request* req, size_t req_size){
	/*printf("sending message\n");
	printf("%u:%u\n", server_info->sin_addr.s_addr, server_info->sin_port);
	printf("req_size: %ld\n", req_size);
	printf("len: %d\n", len);
	printf("sockfd: %d\n", sockfd);*/
	pthread_mutex_lock(&lock);
	activity_last_minute = 1;
	int n = sendto(sockfd, (const void*) req, req_size, 0, (const struct sockaddr*) server_info, len);
	if(n < 0){
		activity_last_minute = 0;
		int err = errno;
		printf("errno: %d\n", err);
		if(err == EBADF){
			printf("EBADF\n");
		}
		if(err == EINVAL){
			printf("EINVAL\n");
		}
		if(err == ENOTSOCK){
			printf("ENOTSOCK\n");
		}
		if(err == ENOTCONN){
			printf("ENOTCONN\n");
		}
	}
	pthread_mutex_unlock(&lock);
	//printf("status: %d\n", n);
}

void handle_cmd_say(struct sockaddr_in* server_info, size_t len, int sockfd, char* cur_channel, char* cur_line){
	struct request_say req;
	req.req_type = REQ_SAY;
	memcpy(&(req.req_channel), cur_channel, CHANNEL_MAX);
	memcpy(&(req.req_text), cur_line, SAY_MAX);
	req.req_text[SAY_MAX - 1] = '\0';
	req.req_channel[CHANNEL_MAX - 1] = '\0';

	send_msg(server_info, len, sockfd, (struct request*) &req, sizeof(struct request_say));
}

void handle_cmd_switch(char* cur_channel, char* cur_line){
	char* channel = get_arg(cur_line);
	if(channel == NULL){
		write(1, "\nERROR: No channel provided\n", 28);
		return;
	}

	memset(cur_channel, '\0', CHANNEL_MAX);
	if(strlen(channel) < CHANNEL_MAX){
		memcpy(cur_channel, channel, strlen(channel));
	}
	else{
		memcpy(cur_channel, channel, CHANNEL_MAX);
		
	}
	cur_channel[CHANNEL_MAX - 1] = '\0';
}

void handle_cmd_exit(){
	free(buffer); //hopefully this frees the entire buffer, not just the request
	pthread_cancel(keep_alive_thread);
	pthread_join(keep_alive_thread, NULL);
	cooked_mode();
	exit(0);
}

void handle_cmd_join(struct sockaddr_in* server_info, size_t len, int sockfd, char* cur_line){
	char* channel = get_arg(cur_line);
	if(channel == NULL){
		write(1, "\nERROR: No channel provided\n", 28);
		return;
	}

	struct request_join req;
	req.req_type = REQ_JOIN;
	memset(&(req.req_channel), '\0', CHANNEL_MAX);
	if(strlen(channel) < CHANNEL_MAX){
		memcpy(&(req.req_channel), channel, strlen(channel));
	}
	else{
		memcpy(&(req.req_channel), channel, CHANNEL_MAX);
	}
	req.req_channel[CHANNEL_MAX - 1] = '\0';

	send_msg(server_info, len, sockfd, (struct request*) &req, sizeof(struct request_join));
	write(1, "\nJoined channel ", 16);
	write(1, req.req_channel, strlen(req.req_channel));
	write(1, "\n", 1);
	//printf("\nJoined channel %s", req.req_channel);
}

void handle_cmd_leave(struct sockaddr_in* server_info, size_t len, int sockfd, char* cur_line){
	char* channel = get_arg(cur_line);
	if(channel == NULL){
		write(1, "\nERROR: No channel provided\n", 28);
		return;
	}

	struct request_leave req;
	req.req_type = REQ_LEAVE;
	memset(&(req.req_channel), '\0', CHANNEL_MAX);
	if(strlen(channel) < CHANNEL_MAX){
		memcpy(&(req.req_channel), channel, strlen(channel));
	}
	else{
		memcpy(&(req.req_channel), channel, CHANNEL_MAX);
	}
	req.req_channel[CHANNEL_MAX - 1] = '\0';

	send_msg(server_info, len, sockfd, (struct request*) &req, sizeof(struct request_leave));
	write(1, "\n Left channel ", 15);
	write(1, req.req_channel, strlen(req.req_channel));
	write(1, "\n", 1);

}

void handle_cmd_list(struct sockaddr_in* server_info, size_t len, int sockfd){
	struct request_list req;
	req.req_type = REQ_LIST;
	send_msg(server_info, len, sockfd, (struct request*) &req, sizeof(struct request_list));
}

void handle_cmd_login(struct sockaddr_in* server_info, size_t len, int sockfd, char* username){
	struct request_login req;
	req.req_type = REQ_LOGIN;
	memset(&(req.req_username), '\0', USERNAME_MAX);
	if(strlen(username) < USERNAME_MAX){
		memcpy(&(req.req_username), username, strlen(username));
	}
	else{
		memcpy(&(req.req_username), username, USERNAME_MAX);
	}
	req.req_username[USERNAME_MAX - 1] = '\0';
	
	send_msg(server_info, len, sockfd, (struct request*) &req, sizeof(struct request_login));
}

void handle_cmd_logout(struct sockaddr_in* server_info, size_t len, int sockfd){
	struct request_logout req;
	req.req_type = REQ_LOGOUT;
	send_msg(server_info, len, sockfd, (struct request*) &req, sizeof(struct request_logout));

}

void handle_cmd_who(struct sockaddr_in* server_info, size_t len, int sockfd, char* cur_line){
	struct request_who req;

	char* channel = get_arg(cur_line);
	if(channel == NULL){
		write(1, "\nERROR: No channel provided\n", 28);
		return;
	}

	req.req_type = REQ_WHO;
	memset(&(req.req_channel), '\0', CHANNEL_MAX);
	if(strlen(channel) < CHANNEL_MAX){
		memcpy(&(req.req_channel), channel, strlen(channel));
	}
	else{
		memcpy(&(req.req_channel), channel, CHANNEL_MAX);
	}
	req.req_channel[CHANNEL_MAX - 1] = '\0';

	send_msg(server_info, len, sockfd, (struct request*) &req, sizeof(struct request_who));
}

void handle_chat(struct sockaddr_in* server_info, size_t len, int sockfd, char* cur_channel, char* cur_line){
	switch(check_if_is_command(cur_line)){
		case CMD_SAY: handle_cmd_say(server_info, len, sockfd, cur_channel, cur_line);
			break;
		case CMD_WHO: handle_cmd_who(server_info, len, sockfd, cur_line);
			break;
		case CMD_LIST: handle_cmd_list(server_info, len, sockfd);
			break;
		case CMD_LEAVE: handle_cmd_leave(server_info, len, sockfd, cur_line);
			break;
		case CMD_SWITCH: handle_cmd_switch(cur_channel, cur_line);
			break;
		case CMD_JOIN: handle_cmd_join(server_info, len, sockfd, cur_line);
			break;
		case CMD_EXIT: 
			handle_cmd_logout(server_info, len, sockfd);
			handle_cmd_exit();
			break;
		default:
			break;
	}
}

struct keep_alive_input {
	struct sockaddr_in* server_info;
	size_t len;
	int sockfd;
};

void* keep_alive_loop(void* info){
	struct keep_alive_input* kai = (struct keep_alive_input*) info;
	struct sockaddr_in* server_info = kai->server_info;
	size_t len = kai->len;
	int sockfd = kai->sockfd;
	int s;
	struct request_keep_alive rka;
	rka.req_type = REQ_KEEP_ALIVE;
	while(1){
		s = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		if(s < 0){
			printf("Unable to set cancel state\n");
		}
		sleep(60);
		s = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if(s < 0){
			printf("Unable to disable cancel\n");
		}
		pthread_mutex_lock(&lock);
		if(activity_last_minute == 0){
			send_msg(server_info, len, sockfd, (struct request*) &rka, sizeof(struct request_keep_alive));
		}
		activity_last_minute = 0;
		pthread_mutex_unlock(&lock);
		
	}
	return NULL;
}

int main(int argc, char** argv){
	if(argc != 4){
		printf("Usage: %s <server_address> <server_port> <username>\n", argv[0]);
		printf("<server_address> = IP address or hostname of the server\n     ");
		printf("<server_port>    = the port of the server process \n          ");
		printf("<username>       = the name this client will connect with\n   ");
	}

	buffer = malloc(BUFFER_MAX);

	char stdinbuf[256];
	int stdinbuf_cur_idx = 0;
	memset(stdinbuf, '\0', 256);

	char cur_channel[CHANNEL_MAX];
	memset(cur_channel, '\0', CHANNEL_MAX);
	strcat(cur_channel, "Common");

	struct sockaddr_in* server_info = setup_server_info(argv[1], argv[2]);

	int sockfd;
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("ERROR: socket creation failed\n");
		exit(1);
	}
	else{
		//printf("socket created\n");
	}

	//printf("logging in\n");
	handle_cmd_login(server_info, sizeof(*server_info), sockfd, argv[3]);
	handle_cmd_join(server_info, sizeof(*server_info), sockfd, "/join Common");


	pthread_mutexattr_t ma;
	pthread_mutexattr_init(&ma);
	pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&(lock), &ma);
	pthread_mutexattr_destroy(&ma);

	struct keep_alive_input kai;
	kai.server_info = server_info;
	kai.len = sizeof(*server_info);
	kai.sockfd = sockfd;
	int pthread_status = pthread_create(&keep_alive_thread, NULL, keep_alive_loop, (void*) &kai);
	if(pthread_status < 0){
		printf("Unable to start keep_alive thread\n");
	}



	//welcome to the nightmare hell zone
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	FD_SET(sockfd, &rfds);

	int select_val;

	raw_mode();
	write(1, ">", 1);
	while(1){
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		select_val = select(sockfd+1, &rfds, NULL, NULL, &tv);
		if(select_val > 2){
			printf("Select returned too many files\n");
		}
		if(FD_ISSET(0, &rfds)){
			//char cur = getchar();
			char temp[2];
			read(0, temp, 1);
			char cur = temp[0];
			//printf("%d\n", (int) cur);
			if(cur == '\n'){
				//printf("\nSTDBUF: |%s|\n", stdinbuf);
				if(stdinbuf[0] != '\0'){
					handle_chat(server_info, sizeof(*server_info), sockfd, cur_channel, stdinbuf);
				}

				if(stdinbuf[0] == '/'){
					//printf("\n");
					write(1, "\n>", 2);
				}
				memset(stdinbuf, '\0', 256);
				stdinbuf_cur_idx = 0;				
			}
			else if(cur == 127){
				if(stdinbuf_cur_idx > 0){
					stdinbuf_cur_idx--;
					stdinbuf[stdinbuf_cur_idx] = '\0';
					write(1, "\b", 1);
					write(1, " ", 1);
					write(1, "\b", 1);
				}
			}
			else{
				if(stdinbuf_cur_idx < 255){
					
					stdinbuf[stdinbuf_cur_idx] = cur;
					stdinbuf_cur_idx++;
					
					char temp2[2];
					temp2[0] = cur;
					temp2[1] = '\0';
					write(1, temp2, 1);
				}
			}
		}
		else if(FD_ISSET(sockfd, &rfds)){
			for(int i = 0; i < 256; i++){
				write(1, "\b", 1);
			}
			for(int i = 0; i < stdinbuf_cur_idx; i++){
				write(1, " ", 1);
			}
			for(int i = 0; i < 256; i++){
				write(1, "\b", 1);
			}
			
			//memset(stdinbuf, '\0', 256);
			//stdinbuf_cur_idx = 0;
			//printf("READING FROM SERVER\n");
			read_from_server(server_info, sockfd, buffer);
			write(1, ">", 1);
			write(1, stdinbuf, stdinbuf_cur_idx);
			
		}


		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		FD_SET(sockfd, &rfds);
	}

	free(server_info);
	free(buffer);

}