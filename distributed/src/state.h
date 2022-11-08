#ifndef STATE_H
#define STATE_H
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdlib>
#include <string.h>

#include "network.h"

struct pair_hash{
    template<class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> &pair) const
    {
        return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
};

class User : public Addressable {
    char name[USERNAME_MAX];
    std::string name_str;
    unsigned int last_seen;
public:
    User(uname_t username_, ip_t ip_, port_t port) : Addressable(ip_, port){
        memcpy(name, username_, USERNAME_MAX);
        name_str = std::string(name);
        last_seen = time(NULL);
    }
    User(uname_t username_, struct sockaddr_in* addr, size_t len) : Addressable(addr, len){
        memcpy(name, username_, USERNAME_MAX);
        name_str = std::string(name);
        last_seen = time(NULL);
    }

    ~User(){}

    char* get_name(){ return name; };
    const std::string& get_name_str(){ return name_str; }

    void update_last_seen(){ last_seen = time(NULL); }
    unsigned int get_last_seen(){ return last_seen;  }

    bool compare_names(uname_t other_name){ return strcmp(name, other_name) == 0; }
    bool compare_names(User* other){ return strcmp(name, other->get_name()) == 0; }
};

class UserSet {
    std::vector<User*> user_list;
    std::unordered_map<std::string, User*> user_name_map;
    std::unordered_map<std::pair<std::string, port_t>, User*, pair_hash> user_ip_map;

    
public:
    UserSet();
    ~UserSet();

    void add_user(User* user);

    User* get_user(uname_t name);
    User* get_user(ip_t ip_, port_t port_);

    //USER MUST BE REMOVED FROM ALL CHANNELS PRIOR TO RUNNING THIS FROM STATE
    void remove_user(uname_t name);
    void remove_user(ip_t ip_, port_t port_);
    void remove_user(User* user);

    std::vector<User*>& get_user_list();
    size_t size();

    void delete_entries();
};

class Server : public Addressable {
public:
    Server(ip_t ip_, port_t port_) : Addressable(ip_, port_) {};
    Server(struct sockaddr_in* addr_, size_t len_) : Addressable(addr_, len_) {};
};

class ServerSet {
    std::vector<Server*> server_list;
    std::unordered_map<std::pair<std::string, port_t>, Server*, pair_hash> server_map;
    
public:
    ServerSet(){};
    ~ServerSet(){};

    void add_server(Server* server);

    Server* get_server(ip_t ip_, port_t port_);
    Server* get_server(struct sockaddr_in* addr, size_t len_);
    void remove_server(ip_t ip_, port_t port_);
    void remove_server(struct sockaddr_in* addr, size_t len_);
    void remove_server(Server* server);

    const std::vector<Server*>* get_server_list();
    size_t size();
    
    void delete_entries();

};

class Channel {
    cname_t name;
    std::string name_str;
    UserSet uset;
    ServerSet sset;
    std::unordered_set<msg_uid_t> uid_set;
    std::unordered_map<std::pair<std::string, port_t>, unsigned int, pair_hash> last_seen;
public:
    Channel(cname_t name_){
        memcpy(name, name_, CHANNEL_MAX);
        name_str = std::string(name_);
    }
    ~Channel();

    char* get_name(){ return name; }
    const std::string& get_name_str(){ return name_str; }

    void add_user(User* user);
    void remove_user(User* user);
    bool contains_user(User* user);
    size_t user_count(){ return uset.size(); }
    const std::vector<User*>& get_user_list(){ return uset.get_user_list(); };

    void add_server(Server* server);
    void remove_server(Server* server);
    void server_heartbeat(Server* server);
    bool contains_server(Server* server);
    size_t server_count(){ return sset.size(); }
    const std::vector<Server*>* get_server_list(){ return sset.get_server_list(); };

    void add_id(msg_uid_t uid){ uid_set.insert(uid); }
    bool contains_id(msg_uid_t uid){ return uid_set.find(uid) != uid_set.end(); }

    bool should_prune(){
        return (uset.size() == 0 && sset.size() <= 1);
    }

    const std::vector<Server*>* get_dead_servers(unsigned int curtime, unsigned int timeout);
};



class ChannelSet {
    std::vector<Channel*> channel_list;
    std::unordered_map<std::string, Channel*> channel_map;
public:
    ChannelSet(){};
    ~ChannelSet(){};

    Channel* get_channel(cname_t name);

    void add_channel(Channel* channel);
    void remove_channel(cname_t name);
    void remove_channel(Channel* channel);

    bool contains_channel(cname_t name);
    bool contains_channel(Channel* channel);

    std::vector<Channel*>* get_channel_list();
    size_t count();

    void delete_entries();

};



class State {
    UserSet uset;
    ChannelSet cset;
    ServerSet sset;
    int sockfd;
    pthread_mutex_t lock;
public:
    State(int sockfd_){ 
        sockfd = sockfd_;
        pthread_mutexattr_t ma;
        pthread_mutexattr_init(&ma);
        pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&lock, &ma);
        pthread_mutexattr_destroy(&ma); 
    }
    ~State(){
        uset.delete_entries();
        cset.delete_entries();
        sset.delete_entries();
    }
    int get_sockfd();

    void add_server(Server* server);

    char* get_username(struct sockaddr_in* addr, size_t len){
        Addressable a = Addressable(addr, len);
        return uset.get_user(a.get_ip_char(), a.get_port())->get_name();
    }

    void cli_login(uname_t uname, struct sockaddr_in* addr, size_t len); //login
    void cli_logout(struct sockaddr_in* addr, size_t len); //logout
    void cli_join(struct sockaddr_in* addr, size_t len, cname_t chan_name); //join
    void cli_leave(struct sockaddr_in* addr, size_t len, cname_t chan_name); //leave
    const std::vector<Channel*>* cli_get_channels(struct sockaddr_in* addr, size_t len); //list, logout (to check for leaves)
    const std::vector<User*>* cli_get_channel_users(struct sockaddr_in* addr, size_t len, cname_t chan_name); //who, say
    void cli_keep_alive(struct sockaddr_in* addr, size_t len); //keep_alive

    void remove_user(User* user);


    //return true if the channel was found locally, false otherwise
    bool s2s_join(struct sockaddr_in* addr, size_t len, cname_t chan_name); //join
    void s2s_leave(struct sockaddr_in* addr, size_t len, cname_t chan_name); //leave
    const std::vector<Server*>* s2s_channel_get_servers(cname_t chan_name); //say
    const std::vector<Server*>* s2s_get_servers(){ return sset.get_server_list(); }
    const std::vector<User*>* s2s_channel_get_users(cname_t chan_name); //say
    const std::vector<Channel*>* s2s_get_channels();
    bool channel_already_got_message(cname_t chan_name, msg_uid_t uid);
    void channel_add_uid(cname_t chan_name, msg_uid_t uid);

    //const std::vector<char*>* channel_class_to_name(const std::vector<Channel*>* channels);
    ChannelSet* get_cset(){ return &cset; }
    UserSet* get_uset(){ return &uset; }
    ServerSet* get_sset(){ return &sset; }

    pthread_mutex_t* get_lock_addr(){ return &lock; }

};



#endif