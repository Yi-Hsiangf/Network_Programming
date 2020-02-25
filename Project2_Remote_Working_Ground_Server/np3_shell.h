#include<iostream>
#include<vector>
#include<sys/stat.h>   
#include<unistd.h>
#include<stdio.h>
#include<string>
#include <string.h>

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


struct Com_content
{
	//shell
	string original_command;
	string command;
	vector<string> parameter;
	int pipe_to_num;
	int pipe_type;
	//fifo
	int FIFO_type;
	int FIFO_to_user;
	int FIFO_from_user;
};

typedef struct client {
	bool 	used;
    int	    id;
    int	    pid;
    int	    port;
	int		open_FIFO_with_id;
	int 	FIFO_fd_table[MAX_CLIENT];
    char	addr[INET6_ADDRSTRLEN];
    char	name[MAX_CLIENT_NAME + 1];
    char	msg[MAX_MSG_SIZE + 1];
} Client;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

// client
void welcome_msg();
void initialize_shared_mem();
void initialize_semaphore();
void client_login(char *client_ipaddr, int client_port);
void client_logout();
void broadcast(char *msg);

// FIFO fuction
bool FIFO_operation(Com_content command_content, int &sender_open_fifo);
bool write_msg_to_fifo(int sender_id, int receiver_id, string fifo_command, int &sender_open_fifo);
void create_fifo(string pathname);
bool get_msg_from_fifo(int sender_id, int receiver_id, string fifo_command);
void delete_fifo(string fifo_pathname);
// client function
void who();
void tell(int tell_id, string tell_string);
void yell(string yell_string);
void name(char *client_ipaddr, int client_port, string client_new_name);


// shell 
void initialize_command(Com_content &command_content);
void NPShell(int sockfd, char* client_ipaddr, int client_port);
vector<string>split_path(string path);
vector<string>split_space(string command);
int exist(const char *name);
void args_execute(Com_content command_content, vector<Pipe_table> vec_pipe_table,  int same_pipe_table_number, pid_t &last_pid);
void childHandler(int signo);