#include<iostream>
#include<vector>
#include<sys/stat.h>   
#include<unistd.h>
#include<stdio.h>
#include<string>
#include <string.h>
#include<map>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<memory.h>
#include<sys/shm.h>         //Used for shared memory
#include <sys/sem.h>		//Used for semaphores

#define SHMKEY          ((key_t) 7999) 				// Shared  memory created
#define	SEMKEY			((key_t) 6999)  			//Semaphore  key
#define SHMFLAG         0666    

#define MAX_MSG_SIZE    1024	// one message has at most 1024 bytes
#define MAX_CLIENT	    31   // 0 is unused
#define MAX_CLIENT_NAME	    20


using namespace std;

class Pipe_table{
	public:
		int fd_in;
		int fd_out;
		int counter;
};

class User_pipe_table{
	public:
		int fd_in;
		int fd_out;
		int user_pipe_exist; 
};


struct Com_content
{
	//shell
	string original_command;
	string command;
	vector<string> parameter;
	int pipe_to_num;
	int pipe_type;
	//fifo
	int user_pipe_type;
	int pipe_to_user;
	int pipe_from_user;
};

struct client {
	bool used;
    int	    id;
    int	    port;
	int 	my_sockfd;
	int		user_pipe_with_id;
	User_pipe_table		user_pipe_table[MAX_CLIENT];
	vector<Pipe_table> vec_pipe_table;
    char	addr[INET6_ADDRSTRLEN];
    char	name[MAX_CLIENT_NAME + 1];
	map<string, string> client_env_map;
};

//server
int find_the_client_sockfd_in_Client_set(int fd_num, vector<client>&Client_set);
 void client_login(client Client, vector<client>&Client_set);
void client_logout(client Client, vector<client>&Client_set, vector<int> &Unused_ID_set, fd_set &afds, int fd_num, int index);
 void initialize_Client(int index, vector<client>&Client_set);

// client
void set_client_env(int uid, vector<client>&Client_set);
void welcome_msg(int client_sockfd);
bool find_exist_user(int id, vector<client>&Client_set);
void broadcast(char *msg, vector<client>&Client_set);

// User pipe fuction
bool user_pipe_operation(int uid, Com_content command_content, vector<client>&Client_set);
bool write_msg_to_user_pipe(int sender_id, int receiver_id, vector<client>&Client_set);
bool get_msg_from_user_pipe(int sender_id, int receiver_id, vector<client>&Client_set);
// client function
void who(int uid, vector<client>&Client_set);
void tell(int uid, int tell_id, string tell_string, vector<client>&Client_set);
void  yell(int uid, string yell_string, vector<client>&Client_set);
void name(int uid, string client_new_name, vector<client>&Client_set);


// shell 
void initialize_command(Com_content &command_content);
bool NPShell(int sockfd, char* buffer, client &Client, vector<client>&Client_set);
vector<string>split_path(string path);
vector<string>split_space(string command);
int exist(const char *name);
void args_execute(Com_content command_content, int same_pipe_table_number, pid_t &last_pid, int uid, vector<client>&Client_set);
void childHandler(int signo);
