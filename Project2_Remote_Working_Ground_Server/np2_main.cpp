#include "np2_shell.h"
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include<map>

using namespace std;
const int backlog = 30;
vector<client> Client_set;

int main(int argc, char* argv[])
{
    int server_stdout, server_stderr;
    fd_set afds; //active file descriptor set //store who is active
    fd_set rfds; //read file descriptor set 
    int nfds; 
    vector<int> Unused_ID_set;
    for(int i=1;i<MAX_CLIENT;i++)
    {
        Unused_ID_set.push_back(i);
    }
    //setenv("PATH", "bin:.", 1);
    for(int i=0;i<MAX_CLIENT;i++)
     {
        client Client;
        Client.used = false;
        Client.id = -1;
        Client.port = -1;
        Client.my_sockfd = -1;
        Client.user_pipe_with_id = -1;
        memset(Client.addr, 0, sizeof(Client.addr));
        memset(Client.name, 0, sizeof(Client.name));
        for(int j=0;j<MAX_CLIENT;j++)
        {
            Client.user_pipe_table[j].fd_in = -1;
            Client.user_pipe_table[j].fd_out = -1;
            Client.user_pipe_table[j].user_pipe_exist = 0;
        }
        Client_set.push_back(Client);
    }

    FD_ZERO(&afds);
    FD_ZERO(&rfds);

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

    // add server socket fd into active file descriptor set
    FD_SET(server_sockfd, &afds);
    cout << "server socketfd : " << server_sockfd << endl;
    nfds = server_sockfd; //nfds should be set to the highest-numbered file descriptor

    while(1)   //for accept the client
    {
        rfds = afds;
         clearenv();
        while((select(nfds+1, &rfds, NULL, NULL, NULL) < 0))
        {
            // some system interupt happened, need errno to indicate the error
            if(errno == EINTR)
                usleep(1000);
            else
            {
                cerr << "Select Error" << endl;
                exit(1);
            }
        }

        for(int fd_num = 0; fd_num < nfds+1; fd_num++)
        {
            if(FD_ISSET(fd_num, &rfds))  //find fd that need to read
            {
                if(fd_num == server_sockfd) //server fd
                {
                    //handle new connections
                    client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_info, &client_addrlen);
                    if(client_sockfd < 0)
                    {
                        cerr << "Accept Error" << endl;  
                    }
                    else 
                    {
                        FD_SET(client_sockfd, &afds); // add to active set
                        if(client_sockfd > nfds)
                            nfds = client_sockfd;

			            char str[INET_ADDRSTRLEN];
			            inet_ntop(client_info.sin_family, &(client_info.sin_addr), str, INET_ADDRSTRLEN);
			            cout << "Accept: " << str << ":" << ntohs(client_info.sin_port) << endl;

                       // calculate the min id
                        //cout << "unused ID: ";
                        int min_id = Unused_ID_set[0];
                        int min_id_index = 0;
                        for(int i=0;i < Unused_ID_set.size();i++)
                        {
                            //cout << Unused_ID_set[i] << " ";
                            if(Unused_ID_set[i] < min_id)
                            {
                                min_id = Unused_ID_set[i];
                                min_id_index = i;
                            }
                        }
                        //cout << "min_id: " << min_id << " at " << min_id_index << endl;
                        //Client information
                        //cout << "client_sockfd: " << client_sockfd << endl;
                        client Client;
                         char client_ipaddr[INET_ADDRSTRLEN];
			            inet_ntop(client_info.sin_family, &(client_info.sin_addr), client_ipaddr, INET_ADDRSTRLEN);
                        Client.port = ntohs(client_info.sin_port);
                        Client.id = min_id;
                        Client.used = true;
                        Client.client_env_map["PATH"] = "bin:.";
                        char client_name[] = "(no name)";
                        strncpy(Client.name, client_name, sizeof(Client.name));
                        strncpy(Client.addr, client_ipaddr, sizeof(Client.addr));

                        Unused_ID_set.erase(Unused_ID_set.begin()+min_id_index);
                        Client.my_sockfd = client_sockfd;
                        Client_set[min_id] = Client;

                        //welcome msg
                        welcome_msg(client_sockfd);
                        //login  & broadcast msg
                        client_login(Client, Client_set);
                        //give client shell "% "
                        char msg[] = "% ";
                        if(send(client_sockfd, msg, strlen(msg),0) == -1)
                        {
                            cerr << "send client % fail" << endl;
                            exit(1);
                        }
		            }
                }
                else //client fd
                {
                    int recv_number_of_byte;
                    char buffer[15001] = {};
                    if((recv_number_of_byte = recv(fd_num,buffer,sizeof(buffer),0)) <= 0)  
                    {
                        // <0: error, 
                        // =0: the client connection have been close
                        int index = find_the_client_sockfd_in_Client_set(fd_num, Client_set);
                        client_logout(Client_set[index], Client_set, Unused_ID_set, afds, fd_num, index);
                                                  
                    }
                    else
                    {
                        // save server stdin, stdout
                        server_stderr = dup(STDIN_FILENO);
                        server_stdout = dup(STDOUT_FILENO);
                        //client get msg
                        dup2(fd_num, STDOUT_FILENO);
                        dup2(fd_num, STDERR_FILENO);
                    
                        
                        int index = find_the_client_sockfd_in_Client_set(fd_num, Client_set);
                        bool client_logout_information = NPShell(fd_num, buffer, Client_set[index], Client_set);
                        if(client_logout_information == true)
                        {
                            client_logout(Client_set[index], Client_set, Unused_ID_set, afds, fd_num, index);
                            dup2(server_stdout, STDOUT_FILENO);
                            dup2(server_stderr, STDERR_FILENO);
                            close(server_stdout);
                            close(server_stderr);
                        }
                        else
                        {
                            dup2(server_stdout,STDOUT_FILENO);
                            dup2(server_stderr,STDERR_FILENO);
                            close(server_stdout);
                            close(server_stderr);
                            char msg[] = "% ";
                            if(send(Client_set[index].my_sockfd, msg, strlen(msg),0) == -1)
                            {
                                cerr << "send client % fail" << endl;
                                exit(1);
                            }   
                        }
                    } 
                }
                
            }
        }
    }   
    close(server_sockfd);
}

 void client_login(client Client, vector<client>&Client_set)
{
    cout << "login" << endl;
    char	msg[MAX_MSG_SIZE + 1];
    snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' entered from %s:%d. ***\n",
                    Client.name, Client.addr, Client.port);
    broadcast(msg, Client_set);
}
void client_logout(client Client, vector<client>&Client_set, vector<int> &Unused_ID_set, fd_set &afds, int fd_num, int index)
{
	char msg[MAX_MSG_SIZE + 1];
    int uid = Client.id;
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' left. ***\n", Client.name);
	broadcast(msg,Client_set);

    Unused_ID_set.push_back(uid); // add the id back to unused set
    FD_CLR(fd_num, &afds); //remove from active set
    initialize_Client(index, Client_set);
    close(fd_num);	
}
 int find_the_client_sockfd_in_Client_set(int fd_num, vector<client>&Client_set)
 {
     for(int i=0;i<Client_set.size();i++)
     {
         if(fd_num == Client_set[i].my_sockfd)
            return i;
     }
     return -1;
 }

 void initialize_Client(int index, vector<client>&Client_set)
{
        Client_set[index].used = false;
        Client_set[index].id = -1;
        Client_set[index].port = -1;
        Client_set[index].my_sockfd = -1;
        Client_set[index].user_pipe_with_id = -1;
        memset(Client_set[index].addr, 0, sizeof(Client_set[index].addr));
        memset(Client_set[index].name, 0, sizeof(Client_set[index].name));
        Client_set[index].client_env_map.clear();
        Client_set[index].client_env_map["PATH"] = "bin:.";
        for(int j=0;j<MAX_CLIENT;j++)
        {
            Client_set[index].user_pipe_table[j].fd_in = -1;
            Client_set[index].user_pipe_table[j].fd_out = -1;
            Client_set[index].user_pipe_table[j].user_pipe_exist = 0;
        }
        Client_set[index].vec_pipe_table.clear();
}




