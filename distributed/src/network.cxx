#include "network.h"


void send_message(int sockfd, const std::vector<Addressable*>* targets, char* buffer, size_t len, const std::string& log_msg){
    if(targets == NULL){
        auto estr = std::string("target set provided to send_message is NULL");
        log_error(estr);
        return;
    }
    auto sendstr = std::string("Sending message to ")+std::to_string(targets->size())+std::string(" destinations");
    log_info(sendstr);

    for(auto target : *targets){
        //auto msg = std::string("sent prepared message");
        log_send(*(target->get_ip_str()), target->get_port(), log_msg);
        auto tinfo = target->get_addr();
        int status = sendto(sockfd, (const void*) buffer, len, 0, (const struct sockaddr*) tinfo.first, tinfo.second);
        if(status < 0){
            auto errstr = std::string("sendto failed (error is ")+std::to_string(status)+std::string(")");
            log_error(errstr);
        }
    }
    
}

void broadcast_say(int sockfd, const std::vector<Addressable*>* targets, cname_t channel_name, uname_t user_name, say_txt_t msg){
    if(targets == NULL){
        auto estr = std::string("target set provided to broadcast_say is NULL");
        log_error(estr);
        return;
    }

    struct text_say say;
    say.txt_type = TXT_SAY;
    memcpy(&(say.txt_username), user_name, USERNAME_MAX);
    memcpy(&(say.txt_channel), channel_name, CHANNEL_MAX);
    memcpy(&(say.txt_text), msg, SAY_MAX);
    say.txt_username[USERNAME_MAX - 1] = '\0';
    say.txt_text[SAY_MAX - 1] = '\0';
    say.txt_channel[CHANNEL_MAX - 1] = '\0';

    std::stringstream ss;
    ss << "Text Say [" << channel_name << "][" << user_name << "]:" << msg;
    //log_network(ss.str());
    send_message(sockfd, targets, (char*) &say, sizeof(struct text_say), ss.str());
}

void broadcast_channel_list(int sockfd, const std::vector<Addressable*>* targets, const std::vector<char*>* channels){
    if(targets == NULL){
        auto estr = std::string("target set provided to broadcast_channel_list is NULL");
        log_error(estr);
        return;
    }

    size_t size = sizeof(struct text_list) + (sizeof(struct channel_info) * channels->size());
    struct text_list* list = (struct text_list*)malloc(size);
    list->txt_type = TXT_LIST;
    list->txt_nchannels = channels->size();
    for(size_t i = 0; i < channels->size(); i++){
        memcpy(list->txt_channels[i].ch_channel, channels->at(i), CHANNEL_MAX);
        list->txt_channels[i].ch_channel[CHANNEL_MAX - 1] = '\0';
    }

    std::stringstream ss;
    ss << "Text List";
    //log_network(ss.str());

    send_message(sockfd, targets, (char*) list, size, ss.str());
    free(list);
}

void broadcast_who(int sockfd, const std::vector<Addressable*>* targets, cname_t cname, const std::vector<char*>* names){
    if(targets == NULL){
        auto estr = std::string("target set provided to broadcast_who is NULL");
        log(NET, ERROR, estr);
        return;
    }

    size_t size = sizeof(struct text_who) + (sizeof(struct user_info) * names->size());
    struct text_who* list = (struct text_who*)malloc(size);
    list->txt_type = TXT_WHO;
    list->txt_nusernames = names->size();
    memcpy(list->txt_channel, cname, CHANNEL_MAX);
    list->txt_channel[CHANNEL_MAX - 1] = '\0';
    for(size_t i = 0; i < names->size(); i++){
        memcpy(list->txt_users[i].us_username, names->at(i), USERNAME_MAX);
        list->txt_users[i].us_username[USERNAME_MAX - 1] = '\0';
    }

    std::stringstream ss;
    ss << "Text Who";
    //log_network(ss.str());

    send_message(sockfd, targets, (char*) list, size, ss.str());
    free(list);
}

void broadcast_error(int sockfd, const std::vector<Addressable*>* targets, say_txt_t msg){
    if(targets == NULL){
        auto estr = std::string("target set provided to broadcast_error is NULL");
        log(NET, ERROR, estr);
        return;
    }

    struct text_error err;
    err.txt_type = TXT_ERROR;
    memcpy(err.txt_error, msg, SAY_MAX);
    err.txt_error[SAY_MAX - 1] = '\0';

    std::stringstream ss;
    ss << "Text Error " << msg;
    //log_network(ss.str());

    send_message(sockfd, targets, (char*) &err, sizeof(struct text_error), ss.str());
}


void broadcast_s2s_join(int sockfd, const std::vector<Addressable*>* targets, cname_t chan_name){
    if(targets == NULL){
        auto estr = std::string("target set provided to broadcast_s2s_join is NULL");
        log(NET, ERROR, estr);
        return;
    }

    struct s2s_join join;
    join.req_type = REQ_S2S_JOIN;
    memcpy(&(join.req_channel), chan_name, CHANNEL_MAX);
    join.req_channel[CHANNEL_MAX - 1] = '\0';

    std::stringstream ss;
    ss << "S2S Join " << chan_name;
    //log_network(ss.str());

    send_message(sockfd, targets, (char*) &join, sizeof(struct s2s_join), ss.str());
}

void broadcast_s2s_leave(int sockfd, const std::vector<Addressable*>* targets, cname_t chan_name){
    if(targets == NULL){
        auto estr = std::string("target set provided to broadcast_s2s_leave is NULL");
        log(NET, ERROR, estr);
        return;
    }

    struct s2s_leave leave;
    leave.req_type = REQ_S2S_LEAVE;
    memcpy(&(leave.req_channel), chan_name, CHANNEL_MAX);
    leave.req_channel[CHANNEL_MAX - 1] = '\0';

    std::stringstream ss;
    ss << "S2S Leave " << chan_name;
    // log_network(ss.str());

    send_message(sockfd, targets, (char*) &leave, sizeof(struct s2s_leave), ss.str());
}

void broadcast_s2s_say(int sockfd, const std::vector<Addressable*>* targets, msg_uid_t uid, uname_t uname, cname_t cname, say_txt_t msg){
    if(targets == NULL){
        auto estr = std::string("target set provided to broadcast_s2s_say is NULL");
        log(NET, ERROR, estr);
        return;
    }

    struct s2s_say say;
    say.req_type = REQ_S2S_SAY;
    memcpy(&(say.req_channel), cname, CHANNEL_MAX);
    memcpy(&(say.req_username), uname, USERNAME_MAX);
    memcpy(&(say.req_text), msg, SAY_MAX);
    say.req_channel[CHANNEL_MAX - 1] = '\0';
    say.req_text[SAY_MAX - 1] = '\0';
    say.req_username[USERNAME_MAX - 1] = '\0';
    say.uid = uid;

    std::stringstream ss;
    ss << "S2S Say " << uid << " [" << cname << "][" << uname << "]:" << msg;
    //log_network(ss.str());

    send_message(sockfd, targets, (char*) &say, sizeof(struct s2s_say), ss.str());
}