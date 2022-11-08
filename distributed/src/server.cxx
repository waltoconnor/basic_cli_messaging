#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#include "state.h"
#include "handlers.h"

#define JOIN_TIMEOUT 120
#define JOIN_TIMEOUT_CHECK_INTERVAL 10

#define JOIN_SEND_INTERVAL 60

#define USER_TIMEOUT_LIMIT 120
#define USER_PRUNE_INTERVAL 10

void handler(State* data, char* buffer, size_t buflen, struct sockaddr_in* cur_client, size_t cli_len){
    //void (*handler)(State*, char*, size_t, struct sockaddr_in*, size_t);
    void (*handler)(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len);
    if(buflen < 4){
        auto estr = std::string("handler recieved invalid input of less than 4 bytes");
        log_error(estr);
        return;
    }
    struct request* req = (struct request*) buffer;
    
    size_t target_size = 4;


    switch(req->req_type){
        case REQ_LOGIN:
            target_size = sizeof(struct request_login);
            handler = handle_login;
            break;
        case REQ_LOGOUT:
            target_size = sizeof(struct request_logout);
            handler = handle_logout;
            break;
        case REQ_JOIN:
            target_size = sizeof(struct request_join);
            handler = handle_join;
            break;
        case REQ_LEAVE:
            target_size = sizeof(struct request_leave);
            handler = handle_leave;
            break;
        case REQ_SAY:
            target_size = sizeof(struct request_say);
            handler = handle_say;
            break;
        case REQ_LIST:
            target_size = sizeof(struct request_list);
            handler = handle_list;
            break;
        case REQ_WHO:
            target_size = sizeof(struct request_who);
            handler = handle_who;
            break;
        case REQ_KEEP_ALIVE:
            target_size = sizeof(struct request_keep_alive);
            handler = handle_keep_alive;
            break;
        case REQ_S2S_JOIN:
            target_size = sizeof(struct s2s_join);
            handler = handle_s2s_join;
            break;
        case REQ_S2S_LEAVE:
            target_size = sizeof(struct s2s_leave);
            handler = handle_s2s_leave;
            break;
        case REQ_S2S_SAY:
            target_size = sizeof(struct s2s_say);
            handler = handle_s2s_say;
            break;
        default:
            std::string errmsg = std::string("handler recieved improper req_type: ")+std::to_string(req->req_type);
            log_error(errmsg);
            return;
    }

    if(target_size != buflen){
        auto estr = std::string("handler recieved req type ")+std::to_string(req->req_type)
        +std::string(" but got wrong number of bytes (target=")+std::to_string(target_size)
        +std::string(" actual=")+std::to_string(buflen)+std::string(")");
        log_error(estr);
        return;
    }

    pthread_mutex_lock(data->get_lock_addr());
    handler(data, buffer, cur_client, cli_len);
    pthread_mutex_unlock(data->get_lock_addr());
}

void server_timeout_loop(State* data){
    while(1){
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        sleep(JOIN_TIMEOUT_CHECK_INTERVAL);
        pthread_setcancelstate(PTHREAD_CANCEL_DEFERRED, NULL);
        pthread_mutex_lock(data->get_lock_addr());
        auto channels = data->s2s_get_channels();

        for(auto c : *channels){
            auto servers = c->get_dead_servers(time(NULL), JOIN_TIMEOUT);
            for(auto s : *servers){
                c->remove_server(s);
            }
            delete servers;
        }
        pthread_mutex_unlock(data->get_lock_addr());
    }
}

void join_send_loop(State* data){
    while(1){
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        sleep(JOIN_SEND_INTERVAL);
        pthread_setcancelstate(PTHREAD_CANCEL_DEFERRED, NULL);
        pthread_mutex_lock(data->get_lock_addr());
        auto channels = data->s2s_get_channels();

        for(auto c : *channels){
            auto servers = c->get_server_list();
            std::vector<Addressable*> avec = std::vector<Addressable*>();
            for(auto s : *servers){
                avec.push_back(s);
            }
            broadcast_s2s_join(data->get_sockfd(), &avec, c->get_name());
        }
        pthread_mutex_unlock(data->get_lock_addr());
    }
}

void user_timeout_loop(State* data){
    while(1){
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        sleep(JOIN_SEND_INTERVAL);
        pthread_setcancelstate(PTHREAD_CANCEL_DEFERRED, NULL);
        pthread_mutex_lock(data->get_lock_addr());
        
        auto users = data->get_uset()->get_user_list();
        for(auto u : users){
            unsigned int last_seen = u->get_last_seen();
            if(time(NULL) - last_seen > USER_TIMEOUT_LIMIT){
                data->remove_user(u);
            }
        }
        pthread_mutex_unlock(data->get_lock_addr());
    }
}

void server_loop(State* data){
    //largest incoming request is s2s_say
    //const size_t BUF_SIZE = sizeof(request_t) + CHANNEL_MAX + SAY_MAX + USERNAME_MAX + sizeof(unsigned long);
    const size_t BUF_SIZE = sizeof(struct s2s_say);
    char buffer[BUF_SIZE];

    struct sockaddr_in cur_client;
    socklen_t len = sizeof(struct sockaddr_in);

    while(1){
        memset(buffer, '\0', BUF_SIZE);

        int bytes_rec = recvfrom(data->get_sockfd(), (char*) buffer, BUF_SIZE, MSG_WAITALL, (struct sockaddr*) &cur_client, &len);
        if(bytes_rec < 0){
            std::string err = std::string("recvfrom recieved packet larger than buffer");
            log_error(err);
            continue;
        }
        handler(data, buffer, bytes_rec, &cur_client, len);
    }
}

void* setup_join_send_loop(void* input){
    State* data = (State*) input;
    join_send_loop(data);
    return NULL;
}

void* setup_server_timeout_loop(void* input){
    State* data = (State*) input;
    server_timeout_loop(data);
    return NULL;
}

void* setup_user_timeout_loop(void* input){
    State* data = (State*) input;
    user_timeout_loop(data);
    return NULL;
}

void setup_loops(State* data){
    pthread_t thread_join_send_loop;
    pthread_t thread_server_timeout_loop;
    pthread_t thread_user_timeout_loop;
    int join_loop_status = pthread_create(&thread_join_send_loop,      NULL, setup_join_send_loop,      (void*) data);
    int s_timeout_status = pthread_create(&thread_server_timeout_loop, NULL, setup_server_timeout_loop, (void*) data);
    int u_timeout_status = pthread_create(&thread_user_timeout_loop  , NULL, setup_user_timeout_loop,   (void*) data);
    if(join_loop_status < 0 || s_timeout_status < 0 || u_timeout_status < 0){
        std::stringstream ss;
        ss << "Failed to make threads (join=" << join_loop_status << ", server_timeout=" << s_timeout_status << ", user_timeout=" << u_timeout_status << ")";
        log_error(ss.str());
        return; 
    }

    server_loop(data);
}

void hostname_or_ip_to_sockaddr_in(char* ip_hostname, char* port, struct sockaddr_in* addr){
    port_t port_int = atoi(port);
    std::stringstream ss;
    ss << "port as parsed by atoi: " << port_int;
    log_info(ss.str());
    //printf("port as parsed by atoi: %u\n", port_int);
    addr->sin_port = htons(port_int);
    addr->sin_family = AF_INET;

    char hostbuffer[256];
    memset(hostbuffer, '\0', 256);
    int hname_status = gethostname(hostbuffer, 256);
    if(hname_status == -1){
        //printf("Error getting hostname\n");
        auto estr = std::string("Error getting hostname");
        log_error(estr);
    }
    else{
        //printf("hostname = %s\n", hostbuffer);
        std::stringstream ss;
        ss << "hostname = " << hostbuffer;
        log_info(ss.str());
    }

    if(strcmp(ip_hostname, hostbuffer) == 0){
        //printf("hostname provided as address, binding to all interfaces\n");
        auto istr = std::string("hostname provided as address, binding to all interfaces");
        log_info(istr);
        addr->sin_addr.s_addr = INADDR_ANY;
    }
    else if(strcmp(ip_hostname, "localhost") == 0){
        int status = inet_aton("127.0.0.1", &(addr->sin_addr));
        if(status == 0){
            //printf("localhost provided, failed to convert to 127.0.0.1\n");
            auto estr = std::string("localhost provided, failed to convert to 127.0.0.1");
            log_error(estr);
        }
        else{
            //printf("localhost provided, binding to 127.0.0.1\n");
            auto istr = std::string("localhost provided, binding to 127.0.0.1");
            log_info(istr);
        }
    }
    else{
        int status = inet_aton(ip_hostname, &(addr->sin_addr));
        if(status == 0){
            //printf("Failed to parse %s as ip\n", ip_hostname);
            //printf("Binding to all interfaces\n");
            auto l1 = std::string("Failed to parse ") + std::string(ip_hostname) + std::string(" as ip");
            auto l2 = std::string("binding to all interfaes");
            log_error(l1);
            log_error(l2);
            addr->sin_addr.s_addr = INADDR_ANY;
        }
        else{
            printf("Bound to %s\n", ip_hostname);
        }
    }
}

int main(int argc, char** argv){
    if((argc - 1) % 2 != 0 || argc < 3){
        printf("IMPROPER STARTUP COMMAND\n");
        printf("Usage: server <IP> <PORT> [<ADJ_IP> <ADJ_PORT> [<ADJ_IP> <ADJ_PORT> [...]]] \n");
        printf("<IP> = local hostname or IP\n");
        printf("<PORT> = local port to use\n");
        printf("<ADJ_IP> = ip or hostname of adjacent server\n");
        printf("<ADJ_PORT> = port of adjacent server\n");
        exit(1);
    }

    struct sockaddr_in local_name;
    hostname_or_ip_to_sockaddr_in(argv[1], argv[2], &local_name);
    auto id_str = std::string(argv[1]) + std::string(":") + std::string(argv[2]);
    server_id = &id_str;

    int sockfd;
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        std::cout << *server_id << "::ERROR::Failed to create to socket" << std::endl;
        exit(1);
    }

    if(bind(sockfd, (const struct sockaddr*) &local_name, sizeof(local_name)) < 0){
        std::cout << *server_id << "::ERROR::Failed to bind to socket" << std::endl;
        exit(1);
    }

    State* state = new State(sockfd);

    struct sockaddr_in temp;
    for(size_t i = 3; i < argc; i+=2){ 
        hostname_or_ip_to_sockaddr_in(argv[i], argv[i+1], &temp);
        std::stringstream ss;
        ss << "Adding server " << argv[i] << ":" << argv[i + 1];
        log_info(ss.str());
        Server* s = new Server(&temp, sizeof(temp));
        state->add_server(s);
    }

    setup_loops(state);

    delete state;
    return 0;
}