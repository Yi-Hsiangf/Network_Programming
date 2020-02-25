#include "np2_shell.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h> //for shared memory
#include <sys/shm.h>

void welcome_msg(int client_sockfd)
{
    char msg[MAX_MSG_SIZE + 1];

    snprintf (msg, MAX_MSG_SIZE + 1,
    "****************************************\n"
    "** Welcome to the information server. **\n"
    "****************************************\n");	
    if(send(client_sockfd, msg, strlen(msg), 0) < 0)
	{
        cerr << "Welcome Send Error" << endl; 
    }
}

