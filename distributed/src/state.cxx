#include "state.h"

void UserSet::remove_user(User* user){
    if(user == NULL){
        //auto estr = std::string("UserSet user not found - ") + user->get_name_str;
        //log_error(estr);
        return;
    }
    user_name_map.erase(user->get_name_str());
    user_ip_map.erase(std::make_pair(*(user->get_ip_str()), user->get_port()));
    for(size_t i = 0; i < user_list.size(); i++){
        if(user_list.at(i)->compare_names(user) && user_list.at(i)->compare_addrs(user)){
            user_list.erase(user_list.begin() + i);
            break;
        }
    }
}

UserSet::UserSet(){

}
UserSet::~UserSet(){

}

void UserSet::add_user(User* user){
    auto in_ip_str = *(user->get_ip_str());
    remove_user(user->get_name());
    remove_user(user->get_ip_char(), user->get_port());
    user_list.push_back(user);
    user_ip_map.insert(std::make_pair(std::make_pair(in_ip_str, user->get_port()), user));
    user_name_map.insert(std::make_pair(std::string(user->get_name()), user));
}

User* UserSet::get_user(uname_t name){
    auto nstr = std::string(name);
    if(user_name_map.find(nstr) == user_name_map.end()){
        return NULL;
    }
    return user_name_map.at(nstr);
}
User* UserSet::get_user(ip_t ip_, port_t port_){
    auto ident = std::make_pair(std::string(ip_), port_);
    if(user_ip_map.find(ident) == user_ip_map.end()){
        return NULL;
    }
    return user_ip_map.at(ident);
}
void UserSet::remove_user(uname_t name){
    User* user = get_user(name);
    if(user == NULL){
        return;
    }
    remove_user(user);
}
void UserSet::remove_user(ip_t ip_, port_t port_){
    User* user = get_user(ip_, port_);
    if(user == NULL){
        return;
    }
    remove_user(user);
}

std::vector<User*>& UserSet::get_user_list(){
    return user_list;
}
size_t UserSet::size(){
    return user_list.size();
}

void UserSet::delete_entries(){
    for(auto u : user_list){
        delete u;
    }
}


void ServerSet::remove_server(Server* server){
    if(server == NULL){
        return;
    }
    server_map.erase(std::make_pair(*(server->get_ip_str()), server->get_port()));
    for(size_t i = 0; i < server_list.size(); i++){
        if(server_list.at(i)->compare_addrs(server)){
            server_list.erase(server_list.begin() + i);
            break;
        }
    }
}

void ServerSet::add_server(Server* server){
    auto struct_pair = server->get_addr();
    remove_server(struct_pair.first, struct_pair.second);
    remove_server(server->get_ip_char(), server->get_port());
    server_list.push_back(server);
    server_map.insert(std::make_pair(std::make_pair(*(server->get_ip_str()), server->get_port()), server));
}
Server* ServerSet::get_server(ip_t ip_, port_t port_){
    auto ip_str = std::string(ip_);
    auto target = std::make_pair(ip_str, port_);
    if(server_map.find(target) == server_map.end()){
        return NULL;
    }
    return server_map.at(target);
}
Server* ServerSet::get_server(struct sockaddr_in* addr, size_t len_){
    Addressable match_obj = Addressable(addr, len_);
    for(auto s : server_list){
        if(match_obj.compare_addrs(s)){
            return s;
        }
    }
    return NULL;
}
void ServerSet::remove_server(ip_t ip_, port_t port_){
    auto server = get_server(ip_, port_);
    if(server == NULL){
        return;
    }
    remove_server(server);
}
void ServerSet::remove_server(struct sockaddr_in* addr, size_t len_){
    auto server = get_server(addr, len_);
    if(server == NULL){
        return;
    }
    remove_server(server);
}

const std::vector<Server*>* ServerSet::get_server_list(){
    return &server_list;
}
size_t ServerSet::size(){
    return server_list.size();
}

void ServerSet::delete_entries(){
    for(auto s : server_list){
        delete s;
    }
}


Channel::~Channel(){

}

void Channel::add_user(User* user){
    uset.remove_user(user->get_name());
    uset.remove_user(user->get_ip_char(), user->get_port());
    uset.add_user(user);
}
void Channel::remove_user(User* user){
    uset.remove_user(user);
}
bool Channel::contains_user(User* user){
    return uset.get_user(user->get_name()) != NULL;
}

void Channel::add_server(Server* server){
    remove_server(server);
    sset.add_server(server);
    last_seen.insert(std::make_pair(std::make_pair(*(server->get_ip_str()), server->get_port()), (unsigned int) time(NULL)));
}
void Channel::remove_server(Server* server){
    sset.remove_server(server->get_ip_char(), server->get_port());
    last_seen.erase(std::make_pair(*(server->get_ip_str()), server->get_port()));
}
bool Channel::contains_server(Server* server){
    return sset.get_server(server->get_ip_char(), server->get_port()) != NULL;
}
void Channel::server_heartbeat(Server* server){
    if(contains_server(server)){
        //last_seen.insert(std::make_pair(std::make_pair(*(server->get_ip_str()), server->get_port()), (unsigned int) time(NULL)));
        std::pair<std::string, port_t> pair = std::make_pair(*(server->get_ip_str()), server->get_port());
        last_seen[pair] = time(NULL);
    }
}

const std::vector<Server*>* Channel::get_dead_servers(unsigned int curtime, unsigned int timeout){
    std::vector<Server*>* servers = new std::vector<Server*>();
    for(auto s : *(sset.get_server_list())){
        unsigned int prev_time = last_seen.at(std::make_pair(*(s->get_ip_str()), s->get_port()));
        if(curtime - prev_time > timeout){
            servers->push_back(s);
        }
    }
    return servers;
}

void ChannelSet::add_channel(Channel* channel){
    if(contains_channel(channel)){
        return;
    }
    channel_list.push_back(channel);
    channel_map.insert(std::make_pair(channel->get_name_str(), channel));
}
Channel* ChannelSet::get_channel(cname_t name){
    if(!contains_channel(name)){
        return NULL;
    }
    return channel_map.at(std::string(name));
}
void ChannelSet::remove_channel(cname_t name){
    auto chan = get_channel(name);
    if(chan == NULL){
        return;
    }
    remove_channel(chan);
}
void ChannelSet::remove_channel(Channel* channel){
    if(channel == NULL){
        return;
    }
    channel_map.erase(channel->get_name_str());
    for(size_t i = 0; i < channel_list.size(); i++){
        if(strcmp(channel_list.at(i)->get_name(), channel->get_name()) == 0){
            channel_list.erase(channel_list.begin() + i);
            return;
        }
    }
}

bool ChannelSet::contains_channel(cname_t name){
    auto nstr = std::string(name);
    return channel_map.find(name) != channel_map.end();
}
bool ChannelSet::contains_channel(Channel* channel){
    return contains_channel(channel->get_name());
}

std::vector<Channel*>* ChannelSet::get_channel_list(){
    return &channel_list;
}
size_t ChannelSet::count(){
    return channel_list.size();
}

void ChannelSet::delete_entries(){
    for(auto c : channel_list){
        delete c;
    }
}


int State::get_sockfd(){
    return sockfd;
}

void State::add_server(Server* server){
    sset.add_server(server);
}

void State::cli_login(uname_t uname, struct sockaddr_in* addr, size_t len){
    User* newuser = new User(uname, addr, len);
    uset.add_user(newuser);
}
void State::cli_logout(struct sockaddr_in* addr, size_t len){
    Addressable adrsbl = Addressable(addr, len);
    User* user = uset.get_user(adrsbl.get_ip_char(), adrsbl.get_port());
    if(user == NULL){
        auto estr = std::string("cli_logout, addresss does not map to user (") + *(adrsbl.get_ip_str()) + std::string(":") + std::to_string(adrsbl.get_port()) + std::string(")");
        log_error(estr);
        //ERROR
        return;
    }
    remove_user(user);
}
void State::cli_join(struct sockaddr_in* addr, size_t len, cname_t chan_name){
    Addressable adrsbl = Addressable(addr, len);
    User* user = uset.get_user(adrsbl.get_ip_char(), adrsbl.get_port());
    if(user == NULL){
        //ERROR
        auto estr = std::string("cli_join, addresss does not map to user (") + *(adrsbl.get_ip_str()) + std::string(":") + std::to_string(adrsbl.get_port()) + std::string(")");
        log_error(estr);
        return;
    }
    user->update_last_seen();
    if(cset.contains_channel(chan_name)){
        cset.get_channel(chan_name)->add_user(user);
    }
    else{
        Channel* nchan = new Channel(chan_name);
        nchan->add_user(user);
        cset.add_channel(nchan);
    }
}
void State::cli_leave(struct sockaddr_in* addr, size_t len, cname_t chan_name){
    Addressable adrsbl = Addressable(addr, len);
    User* user = uset.get_user(adrsbl.get_ip_char(), adrsbl.get_port());
    if(user == NULL){
        //ERROR
        auto estr = std::string("cli_leave, addresss does not map to user (") + *(adrsbl.get_ip_str()) + std::string(":") + std::to_string(adrsbl.get_port()) + std::string(")");
        log_error(estr);
        return;
    }
    user->update_last_seen();
    if(cset.contains_channel(chan_name)){
        cset.get_channel(chan_name)->remove_user(user);
    }
    else{
        //ERROR
        auto estr = std::string("cli_leave, channel does not exist: ") + std::string(chan_name);
        log_error(estr); 
        return;
    }
}
const std::vector<Channel*>* State::cli_get_channels(struct sockaddr_in* addr, size_t len){
    Addressable adrsbl = Addressable(addr, len);
    User* user = uset.get_user(adrsbl.get_ip_char(), adrsbl.get_port());
    if(user == NULL){
        //ERROR
        auto estr = std::string("cli_get_channels, addresss does not map to user (") + *(adrsbl.get_ip_str()) + std::string(":") + std::to_string(adrsbl.get_port()) + std::string(")");
        log_error(estr);
        return NULL;
    }
    user->update_last_seen();

    return cset.get_channel_list();
}
const std::vector<User*>* State::cli_get_channel_users(struct sockaddr_in* addr, size_t len, cname_t chan_name){
    Addressable adrsbl = Addressable(addr, len);
    User* user = uset.get_user(adrsbl.get_ip_char(), adrsbl.get_port());
    if(user == NULL){
        //ERROR
        auto estr = std::string("cli_get_channel_users, addresss does not map to user (") + *(adrsbl.get_ip_str()) + std::string(":") + std::to_string(adrsbl.get_port()) + std::string(")");
        log_error(estr);
        return NULL;
    }
    user->update_last_seen();

    if(!cset.contains_channel(chan_name)){
        return NULL;
    }
    return &(cset.get_channel(chan_name)->get_user_list());
}
void State::cli_keep_alive(struct sockaddr_in* addr, size_t len){
    Addressable adrsbl = Addressable(addr, len);
    User* user = uset.get_user(adrsbl.get_ip_char(), adrsbl.get_port());
    if(user == NULL){
        //ERROR
        auto estr = std::string("cli_keep_alive, addresss does not map to user (") + *(adrsbl.get_ip_str()) + std::string(":") + std::to_string(adrsbl.get_port()) + std::string(")");
        log_error(estr);
        return;
    }
    user->update_last_seen();
}

void State::remove_user(User* user){
    if(user == NULL){
        //ERROR
        auto estr = std::string("remove_user, user does not exist: ") + user->get_name_str();
        log_error(estr);
        return;
    }
    //remove from channels and then remove from user list
    auto clist = cset.get_channel_list();
    for(auto c : *clist){
        c->remove_user(user);
    }
    uset.remove_user(user);
}

bool State::s2s_join(struct sockaddr_in* addr, size_t len, cname_t chan_name){
    Server* srvr = sset.get_server(addr, len);
    if(srvr == NULL){
        //ERROR
        //RETURN TRUE TO STOP FORWARDING OF MESSAGE
        auto eaddr = *(srvr->get_ip_str()) + std::to_string(srvr->get_port());
        auto estr = std::string("s2s_join got message from server not on adj list: ") + eaddr;
        log_error(estr);
        return true;
    }
    auto chan = cset.get_channel(chan_name);
    if(chan != NULL){
        if(chan->contains_server(srvr)){
            chan->server_heartbeat(srvr);
        }
        else{
            chan->add_server(srvr);
            chan->server_heartbeat(srvr);
        }
        return true;
    }
    else{
        Channel* nchan = new Channel(chan_name);
        nchan->add_server(srvr);
        cset.add_channel(nchan);
        return false;
    }
    
}
void State::s2s_leave(struct sockaddr_in* addr, size_t len, cname_t chan_name){
    Server* srvr = sset.get_server(addr, len);
    if(srvr == NULL){
        auto eaddr = *(srvr->get_ip_str()) + std::to_string(srvr->get_port());
        auto estr = std::string("s2s_leave got message from server not on adj list: ") + eaddr;
        log_error(estr);
        //ERROR
        return;
    }
    auto chan = cset.get_channel(chan_name);
    if(chan == NULL){
        //ERROR
        auto err = std::string("Channel does not exist: ") + std::string(chan_name);
        log_error(err);
        return;
    }
    chan->remove_server(srvr);
}
const std::vector<Server*>* State::s2s_channel_get_servers(cname_t chan_name){
    auto chan = cset.get_channel(chan_name);
    if(chan == NULL){
        auto err = std::string("Channel does not exist: ") + std::string(chan_name);
        log_error(err);
        return NULL;
    }
    return chan->get_server_list();
}

bool State::channel_already_got_message(cname_t chan_name, msg_uid_t uid){
    auto chan = cset.get_channel(chan_name);
    if(chan == NULL){
        return NULL;
    }
    return chan->contains_id(uid);
}
void State::channel_add_uid(cname_t chan_name, msg_uid_t uid){
    auto chan = cset.get_channel(chan_name);
    if(chan == NULL){
        return;
    }
    chan->add_id(uid);
}

const std::vector<User*>* State::s2s_channel_get_users(cname_t chan_name){
    auto chan = cset.get_channel(chan_name);
    if(chan == NULL){
        return NULL;
    }
    return &(chan->get_user_list());
}

const std::vector<Channel*>* State::s2s_get_channels(){
    return cset.get_channel_list();
}

/*const std::vector<char*>* State::channel_class_to_name(const std::vector<Channel*>* channels){
    auto vec = new std::vector<char*>();
    for(size_t i = 0; i < channels->size(); i++){
        vec->push_back(channels->at(i)->get_name());
    }
    return vec;
}*/