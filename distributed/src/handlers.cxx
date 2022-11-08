#include "handlers.h"

//this is /dev/urandom
std::random_device dev;
std::mt19937_64 mersenne_twister_64_bits = std::mt19937_64(dev());
std::uniform_int_distribution<unsigned long> rand64bit;

unsigned long gen_uid(){
    return rand64bit(mersenne_twister_64_bits);
}

void channel_pruning_procedure(State* data, const std::vector<Channel*>* channels){
    
    auto to_prune = std::vector<Channel*>();
    
    for(auto c : *channels){
        if(c->should_prune()){
            to_prune.push_back(c);
        }
    }

    for(size_t i = 0; i < to_prune.size(); i++){
        auto tvec = std::vector<Addressable*>();
        for(auto s : *(to_prune.at(i)->get_server_list())){
            tvec.push_back(s);
        }
        std::stringstream ss;
        ss << "purning channel " << to_prune.at(i)->get_name() << " "<< to_prune.at(i)->user_count() << "users," << to_prune.at(i)->server_count() << " servers)";
        log_info(ss.str());
        broadcast_s2s_leave(data->get_sockfd(), &tvec, to_prune.at(i)->get_name());
    }

    for(auto c : to_prune){
        data->get_cset()->remove_channel(c);
        delete c;
    }
}


void handle_login(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct request_login* req = (struct request_login*) buffer;
    req->req_username[USERNAME_MAX - 1] = '\0';

    std::stringstream ss;
    ss << "Request Login " << req->req_username;
    log_recv(cur_client, ss.str());

    data->cli_login(req->req_username, cur_client, cli_len);
}

void handle_logout(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct request_logout* req = (struct request_logout*) buffer;
    data->cli_logout(cur_client, cli_len);

    std::stringstream ss;
    ss << "Request Logout";
    log_recv(cur_client, ss.str());

    auto channels = data->s2s_get_channels(); //use s2s because the user is now gone
    channel_pruning_procedure(data, channels);
}

void handle_join(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct request_join* req = (struct request_join*) buffer;
    req->req_channel[CHANNEL_MAX - 1] = '\0';
    data->cli_join(cur_client, cli_len, req->req_channel);
    auto tvec = std::vector<Addressable*>();
    auto servers = data->s2s_get_servers();
    for(auto s : *servers){
        tvec.push_back(s);
    }

    std::stringstream ss;
    ss << "Request Join " << req->req_channel;
    log_recv(cur_client, ss.str());

    broadcast_s2s_join(data->get_sockfd(), &tvec, req->req_channel);
}

void handle_leave(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct request_leave* req = (struct request_leave*) buffer;
    req->req_channel[CHANNEL_MAX - 1] = '\0';
    data->cli_leave(cur_client, cli_len, req->req_channel);
    auto channels = data->cli_get_channels(cur_client, cli_len);

    std::stringstream ss;
    ss << "Request Leave " << req->req_channel;
    log_recv(cur_client, ss.str());

    channel_pruning_procedure(data, channels);
}

void handle_say(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct request_say* req = (struct request_say*) buffer;
    req->req_channel[CHANNEL_MAX - 1] = '\0';
    req->req_text[SAY_MAX - 1] = '\0';
    char* username = data->get_username(cur_client, cli_len);

    auto targets = data->cli_get_channel_users(cur_client, cli_len, req->req_channel);
    auto tvec = std::vector<Addressable*>();
    for(auto u : *targets){
        tvec.push_back(u);
    }

    std::stringstream ss;
    ss << "Request Say " << req->req_channel << " \"" << req->req_text << "\"";
    log_recv(cur_client, ss.str());

    broadcast_say(data->get_sockfd(), &tvec, req->req_channel, username, req->req_text);

    auto stargets = data->s2s_channel_get_servers(req->req_channel);
    auto stvec = std::vector<Addressable*>();
    for(auto u : *stargets){
        stvec.push_back(u);
    }


    broadcast_s2s_say(data->get_sockfd(), &stvec, gen_uid(), username, req->req_channel, req->req_text);
}

void handle_list(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct request_list* req = (struct request_list*) buffer;
    auto a = Addressable(cur_client, cli_len);
    auto vec = std::vector<Addressable*>();
    vec.push_back(&a);

    auto channels = data->cli_get_channels(cur_client, cli_len);
    auto cnamevec = std::vector<char*>();
    for(auto c : *channels){
        cnamevec.push_back(c->get_name());
    }

    std::stringstream ss;
    ss << "Request List";
    log_recv(cur_client, ss.str());

    broadcast_channel_list(data->get_sockfd(), &vec, &cnamevec);
}

void handle_who(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct request_who* req = (struct request_who*) buffer;
    req->req_channel[CHANNEL_MAX - 1] = '\0';

    auto a = Addressable(cur_client, cli_len);
    auto vec = std::vector<Addressable*>();
    vec.push_back(&a);

    auto users = data->cli_get_channel_users(cur_client, cli_len, req->req_channel);
    auto unamevec = std::vector<char*>();
    for(auto u : *users){
        unamevec.push_back(u->get_name());
    }

    std::stringstream ss;
    ss << "Request Who " << req->req_channel;
    log_recv(cur_client, ss.str());

    broadcast_who(data->get_sockfd(), &vec, req->req_channel,  &unamevec);
}

void handle_keep_alive(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    data->cli_keep_alive(cur_client, cli_len);
}

void handle_s2s_join(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct s2s_join* req = (struct s2s_join*) buffer;
    req->req_channel[CHANNEL_MAX - 1] = '\0';

    std::stringstream ss;
    ss << "S2S Join " << req->req_channel;
    log_recv(cur_client, ss.str());

    bool is_terminal = data->s2s_join(cur_client, cli_len, req->req_channel);
    if(!is_terminal){
        auto stargets = data->s2s_get_servers();
        auto stvec = std::vector<Addressable*>();
        for(auto u : *stargets){
            stvec.push_back(u);
        }
        
        broadcast_s2s_join(data->get_sockfd(), &stvec, req->req_channel);
    }
}

void handle_s2s_say(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct s2s_say* req = (struct s2s_say*) buffer;
    req->req_channel[CHANNEL_MAX - 1] = '\0';
    req->req_text[SAY_MAX - 1] = '\0';
    req->req_username[USERNAME_MAX - 1] = '\0';

    auto sender = Addressable(cur_client, cli_len);
    auto sender_only_vec = std::vector<Addressable*>();
    sender_only_vec.push_back(&sender);

    std::stringstream ss;
    ss << "S2S Say " << req->req_username << " " << req->req_channel << " \"" << req->req_text << "\"";
    log_recv(cur_client, ss.str());

    bool channel_already_got_message = data->channel_already_got_message(req->req_channel, req->uid);
    if(channel_already_got_message){
        auto lstr = std::string("Leaving channel since no users other than sender");
        log_info(lstr);
        broadcast_s2s_leave(data->get_sockfd(), &sender_only_vec, req->req_channel);
        return;
    }
    

    auto tvec = std::vector<Addressable*>();
    for(auto s : *(data->s2s_channel_get_servers(req->req_channel))){
        if(sender.compare_addrs(s)){
            continue;
        }
        tvec.push_back(s);
    }

    broadcast_s2s_say(data->get_sockfd(), &tvec, req->uid, req->req_username, req->req_channel, req->req_text);
    
    auto tuvec = std::vector<Addressable*>();
    for(auto u : *(data->s2s_channel_get_users(req->req_channel))){
        tuvec.push_back(u);
    }

    broadcast_say(data->get_sockfd(), &tuvec, req->req_channel, req->req_username, req->req_text);

    channel_pruning_procedure(data, data->s2s_get_channels());

}

void handle_s2s_leave(State* data, char* buffer, struct sockaddr_in* cur_client, size_t cli_len){
    struct s2s_leave* req = (struct s2s_leave*) buffer;
    req->req_channel[CHANNEL_MAX - 1] = '\0';

    std::stringstream ss;
    ss << "S2S Leave " << req->req_channel;
    log_recv(cur_client, ss.str());

    data->s2s_leave(cur_client, cli_len, req->req_channel);
}