#include "np3_shell.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<memory.h>
#include<sys/shm.h>         //Used for shared memory
#include <sys/sem.h>		//Used for semaphores
#include <sys/types.h>
#include <sys/stat.h>

#define BACKLOG 30 

using namespace std;

static void server_sigHandler(int signo);
static void create_shared_mem();
static void create_semaphore(); 

const int backlog = 1;

int main(int argc, char* argv[])
{
    //signal for server
    signal(SIGCHLD, server_sigHandler);
    signal(SIGINT , server_sigHandler);
    signal(SIGQUIT, server_sigHandler);
    signal(SIGTERM, server_sigHandler);

    // socket
    int  server_sockfd, client_sockfd;
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_sockfd == -1) 
    {
		cerr << "Create Socket Fail!" << endl;
	}    

    // config
    struct sockaddr_in server_info, client_info;
    bzero(&server_info, sizeof(server_info));  //initialize
    server_info.sin_family = PF_INET;; //ipv4
    server_info.sin_addr.s_addr = INADDR_ANY;
    server_info.sin_port = htons(atoi(argv[1]));
    socklen_t client_addrlen = sizeof(client_info);

    int bool_flag = 1;
    if(setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &bool_flag, sizeof(int)) < 0)
    {
		cerr << " Set Sock Error" << endl;
	}
	if(setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEPORT, &bool_flag, sizeof(int)) < 0)
    {
		cerr << " Set Sock Error" << endl;
	}

    //bind
    if(bind(server_sockfd, (struct sockaddr*)&server_info, sizeof(server_info)) < 0)
    {
        cerr << "Bind Error" << endl;    
    }
    //listen
    if(listen(server_sockfd, backlog) < 0)
    {
        cerr << "Listen Error" << endl;  
        exit(0);
    }

    // create semaphore and shared memory
    create_semaphore();
    create_shared_mem();

    while(1)
    {
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_info, &client_addrlen);
    
        if(client_sockfd < 0)
        {
            cerr << "Accept Error" << endl;  
        }
        else 
        {
			char str[INET_ADDRSTRLEN];
			inet_ntop(client_info.sin_family, &(client_info.sin_addr), str, INET_ADDRSTRLEN);
			cout << "Accept: " << str << ":" << ntohs(client_info.sin_port)<< endl;
		}
    
        //fork 
        int child_pid;
        if((child_pid = fork()) < 0)
        {
            cerr << "Fork Child Error" << endl; 
        }
    
        if(child_pid == 0)
        {
            //get client address
            char client_ipaddr[INET_ADDRSTRLEN];
			inet_ntop(client_info.sin_family, &(client_info.sin_addr), client_ipaddr, INET_ADDRSTRLEN);
            //get client port
            int client_port = ntohs(client_info.sin_port);
            //child do not need listen
            close(server_sockfd);
            //child
            //cout << "In child" << endl;
			dup2(client_sockfd, STDOUT_FILENO);
			dup2(client_sockfd, STDERR_FILENO);
            NPShell(client_sockfd, client_ipaddr, client_port);

        }
        else
        {
            //parent
            close(client_sockfd);	
        }
    }   
    close(server_sockfd);
    return 0;
}


void create_shared_mem()
{
    int shmid = 0;
    Client *Client_set = nullptr;
    // allocated the shared memory
    if ((shmid = shmget (SHMKEY, MAX_CLIENT * sizeof (Client), 0666 | IPC_CREAT)) < 0) 
    {
        perror("Shmget Error");
        exit(1);
    } 
    // attached the shared memory
    if ((Client_set = (Client *) shmat (shmid, nullptr, 0)) == (Client *) -1) 
    {
        perror("Shmat Error");
        exit(1);
    }
    // initialize the memory 
    memset(Client_set, 0, MAX_CLIENT * sizeof (Client));
    shmdt(Client_set);
}

void create_semaphore()
{
    int semid = 0;
    if((semid = semget(SEMKEY, 1, IPC_CREAT|0666)) < 0)
    {
        perror("Semaphore Semget Error");
        exit(1);    
    }
    cout << "semid ID: " << semid << endl;

    union semun sem_union_init;
    sem_union_init.val = 1;
    if(semctl(semid, 0, SETVAL, sem_union_init) == -1)
    {
        perror("Semaphore Semctl Error");
        exit(1);
    }

}

void server_sigHandler(int signo)
{
    //child of server (fork to client)
    if(signo == SIGCHLD)
    {
        while (waitpid (-1, NULL, WNOHANG) > 0);      
    }
    else if(signo == SIGINT || signo == SIGQUIT || signo == SIGTERM)
    {
        int shmid;
        if((shmid = shmget (SHMKEY, MAX_CLIENT * sizeof (Client), SHMFLAG)) < 0)
        {
            cerr << "shmid:"  <<  shmid << endl;
            perror("Server_sigHandler Shmget Error");
            exit(1);
        }
        // remove shared memory
        // shmctl : control shared memory
        // IPC_RMID : Mark the segment to be destroyed
        if (shmctl (shmid, IPC_RMID, nullptr) < 0) 
        {
            perror("Server_sigHandler Shmctl Error");
            exit(1);
        }

        // removed semaphore
        int semid;
        if ((semid =  semget((key_t)SEMKEY, 1, 0666)) < 0)
        {
            perror("Server_sigHandler Semget Error");
            exit(1);
        }
        union semun sem_union_delete;
        if (semctl(semid, 0, IPC_RMID, sem_union_delete) == -1)
        {
            perror("Server_sigHandler Semget Error");
            exit(1);
        }      
        exit(0);
    }

}