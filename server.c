#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#include "proto.h"
#include "raw.h"
#include "state.h"
#include "server_helpers.h"


#define MAX_CHANNEL_COUNT 1024
#define MAX_USERS_PER_CHANNEL 1024

int test_valid(char* buffer, size_t nbytes){
	struct request* req = (struct request*) buffer;
	size_t target_size = 0;
	switch(req->req_type){
		case REQ_LOGIN: target_size = sizeof(struct request_login);
			break;
		case REQ_LOGOUT: target_size = sizeof(struct request_logout);
			break;
		case REQ_JOIN: target_size = sizeof(struct request_join);
			break;
		case REQ_LEAVE: target_size = sizeof(struct request_leave);
			break;
		case REQ_SAY: target_size = sizeof(struct request_say);
			break;
		case REQ_LIST: target_size = sizeof(struct request_list);
			break;
		case REQ_WHO: target_size = sizeof(struct request_who);
			break;
		case REQ_KEEP_ALIVE: target_size = sizeof(struct request_keep_alive);
			break;
		default: printf("ERROR: test_valid recieved packet with invalid req_type, flagging for discard\n");
			target_size = 0;
	}

	return nbytes == target_size;
}

void handle_request(struct state* data, char* buffer, size_t len, struct sockaddr_in* cur_client){
	struct request* req = (struct request*) buffer;
	switch(req->req_type){
		case REQ_LOGIN: handle_login(data, buffer, len, cur_client);
			break;
		case REQ_LOGOUT: handle_logout(data, buffer, len, cur_client);
			break;
		case REQ_JOIN: handle_request_join(data, buffer, len, cur_client);
			break;
		case REQ_LEAVE: handle_request_leave(data, buffer, len, cur_client);
			break;
		case REQ_SAY: handle_request_say(data, buffer, len, cur_client);
			break;
		case REQ_LIST: handle_request_list(data, buffer, len, cur_client);
			break;
		case REQ_WHO: handle_request_who(data, buffer, len, cur_client);
			break;
		case REQ_KEEP_ALIVE: handle_request_keep_alive(data, buffer, len, cur_client);
			break;
		default: printf("ERROR: Recieved packet with bad request type (type = %d)\n", req->req_type);
	}

}


void server_loop(struct state* data){
	//largest possible message going in to the server is say_request
	//4 byte type + 32 byte channel name + 64 byte text field
	const size_t BUF_SIZE = sizeof(request_t) + CHANNEL_MAX + SAY_MAX;
	char buffer[BUF_SIZE];
	memset(buffer, '\0', BUF_SIZE);

	struct sockaddr_in cur_client;

	//printf("Initial is %s: %s\n", inet_ntoa(cur_client.sin_addr), buffer);

	socklen_t len = sizeof(struct sockaddr_in);
	

	while(1){
		memset(buffer, '\0', BUF_SIZE);
		
		int bytes_recieved = recvfrom(data->sockfd, (char*) buffer, BUF_SIZE, MSG_WAITALL, (struct sockaddr*) &cur_client, &len);
		
		printf("Got request from %s: %d bytes\n", inet_ntoa(cur_client.sin_addr), bytes_recieved);
		if(bytes_recieved < 0){
			printf("ERROR: recieved too large of packet\n");
			bubble_up_error(data, "packet caused failure of recvfrom, likely too large for buffer", len, &cur_client);
			continue;
		}
		if(test_valid(buffer, bytes_recieved)){
			//only lock datastructure if input was valid
			pthread_mutex_lock(&(data->lock));
			handle_request(data, buffer, len, &cur_client);
			pthread_mutex_unlock(&(data->lock));
		}
		else{
			bubble_up_error(data, "actual packet size differs from size indicated by req_type", len, &cur_client);
		}

	}

}

void* cleanup_loop(void* ctx){
	struct state* data = (struct state*) ctx;

	int s;
	while(1){
		s = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		if(s < 0){
			printf("Unable to set cancel state\n");
		}
		sleep(120);
		s = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		if(s < 0){
			printf("Unable to disable pthread cancel\n");
		}
		pthread_mutex_lock(&(data->lock));
		scan_and_remove_inactive_users(data);
		pthread_mutex_unlock(&(data->lock));
	}
	return NULL;
}

int setup_server(char* ip_hostname, char* port){
	int sockfd;
	uint16_t port_int = atoi(port);
	printf("port as parsed by atoi: %u\n", port_int);

	struct sockaddr_in servaddr;


	char hostbuffer[256];
	memset(hostbuffer, '\0', 256);
	int hname_status = gethostname(hostbuffer, 256);
	if(hname_status == -1){
		printf("Error getting hostname\n");
	}
	else{
		printf("hostname = %s\n", hostbuffer);
	}

	if(strcmp(ip_hostname, hostbuffer) == 0){
		printf("hostname provided as address to bind, going to bind all interfaces\n");
		servaddr.sin_addr.s_addr = INADDR_ANY;
	}
	else if(strcmp(ip_hostname, "localhost") == 0){
		printf("trying localhost\n");
		int status = inet_aton("127.0.0.1", &(servaddr.sin_addr));
		if(status == 0){
			printf("Failed to convert localhost to 127.0.0.1\n");
			printf("Binding to all interfaces\n");
			servaddr.sin_addr.s_addr = INADDR_ANY;
		}
		else{
			printf("Setting server address to 127.0.0.1 (localhost)\n");
		}
	}
	else{
		printf("trying ip\n");
		int status = inet_aton(ip_hostname, &(servaddr.sin_addr));
		printf("converted ip, status = %d\n", status);
		if(status == 0){
			printf("Failed to parse %s as ip\n", ip_hostname);
			printf("Binding to all interfaces\n");
			servaddr.sin_addr.s_addr = INADDR_ANY;
		}
		else{
			printf("Bound to local interface at %s\n", ip_hostname);
		}
	}
	
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("ERROR: failed to create socket\n");
		exit(1);
	}

	memset(&servaddr, 0 , sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	//servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port_int);

	if(bind(sockfd, (const struct sockaddr*) &servaddr, sizeof(servaddr)) < 0){
		printf("ERROR: failed to bind to socket\n");
		exit(1);
	}

	return sockfd;

}

int main(int argc, char** argv){
	if(argc != 3){
		printf("Usage: %s <ip/hostname> <port>\n", argv[0]);
		printf("<ip/hostname> = either the IP address of a local interface to bind to, localhost to bind to 127.0.0.1, or the hostname to bind to all local interfaces (INADDR_ANY)\n");
		printf("<port> = the port to bind to\n");
	}

	int sockfd = setup_server(argv[1], argv[2]);
	struct state* data = alloc_state(sockfd);

	pthread_mutexattr_t ma;
	pthread_mutexattr_init(&ma);
	pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&(data->lock), &ma);
	pthread_mutexattr_destroy(&ma);

	int pthread_status = pthread_create(&(data->cleanup_thread), NULL, cleanup_loop, (void*) data);
	if(pthread_status < 0){
		printf("ERROR: unable to start cleanup loop\n");
	}
	server_loop(data);

}