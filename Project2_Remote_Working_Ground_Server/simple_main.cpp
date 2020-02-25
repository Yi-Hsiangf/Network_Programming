#include "npshell.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>  //structure of sockaddr_in

using namespace std;

const int backlog = 1;

int main(int argc, char* argv[])
{
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
			cout << "Accept: " << str << endl;
		}
    
        //fork 
        int child_pid;
        if((child_pid = fork()) < 0)
        {
            cerr << "Fork Child Error" << endl; 
        }
    
        if(child_pid == 0)
        {
            //child
            //cout << "In child" << endl;
			dup2(client_sockfd, STDOUT_FILENO);
			dup2(client_sockfd, STDERR_FILENO);

            NPShell(client_sockfd);

            close(client_sockfd);
        }
        else
        {
            //parent
            close(client_sockfd);	
        }
    }   
    close(server_sockfd);
}