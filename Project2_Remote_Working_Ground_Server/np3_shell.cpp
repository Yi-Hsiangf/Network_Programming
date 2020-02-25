#include<vector>
#include<sys/stat.h>   
#include<unistd.h>
#include<stdio.h>
#include <sstream>
#include <iterator>
#include<string>
#include<string.h>
#include <signal.h>  
#include <errno.h>  
#include <sys/types.h>  
#include <sys/wait.h> 
#include<map> 
#include <fcntl.h>
#include<stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<memory.h>
#include<sys/shm.h>         //Used for shared memory
#include <sys/sem.h>		//Used for semaphores

#include <sys/types.h>
#include <sys/stat.h>

#include "np3_shell.h"

using namespace std;

//Shared Memorey
static Client * Client_set;
static int shmid = 0, semid = 0, uid = 0;
static bool access_semaphore();
static bool release_semaphore();
static void clientHandler(int signo);
static void FIFO_sigHandler(int signo);

// pipe type
const int no_pipe_exclamtion = 0;
const int include_pipe = 1;
const int include_exclamation = 2;
const int include_input_text = 3;
// FIFO type
const int only_write_to_FIFO = 4;
const int only_read_from_FIFO = 5;
const int both_read_and_write_FIFO = 6;

vector<pid_t> pid_table;

void NPShell(int sockfd, char* client_ipaddr, int client_port)
{  
	string Command; 
	//string PATH = "bin:.";
	vector<Com_content> vec_command;
	vector<string> split_command;
	
	vector<Pipe_table> vec_pipe_table;  // vector for storing pipetable

	setenv("PATH", "bin:.", 1);
	
	welcome_msg();
	initialize_shared_mem();
	initialize_semaphore();
	signal(SIGUSR1, clientHandler);	// receive others' msg
    signal(SIGINT , clientHandler); // exit msg from client
    signal(SIGQUIT, clientHandler);
    signal(SIGTERM, clientHandler);
	signal(SIGUSR2, FIFO_sigHandler);	// FIFO signal

	client_login(client_ipaddr, client_port);

	while(cout << "% " << flush)
	{
		char buffer[15001] = {};
		if(recv(sockfd,buffer,sizeof(buffer),0) < 0)
		{
			cerr << "Receive Error" << endl;  
		}
		else
		{
				string Command = string(buffer);
				//cout << Command.size() << endl;
				int size = Command.size()-1;
				for(int i = size; i >= 0;i--)
				{
					if(Command[i] == '\r' || Command[i] == '\n')
						Command.erase(Command.begin()+i);
				}
				
				if(Command.size() == 0)
					continue;
				split_command = split_space(Command);

		/* ------------------------printenv,setenv-------------------------------*/		
				if(split_command[0] == "printenv")
				{
					char *env = getenv(split_command[1].c_str());
					if(env == NULL)
					{
						continue;
					}
					cout << env << endl;
					split_command.clear();
					continue;
				}
				else if(split_command[0] == "setenv")
				{
					//map_variable_to_path[split_command[1]] = split_command[2];
					setenv(split_command[1].c_str(), split_command[2].c_str(), 1);
					split_command.clear();
					continue;
				}
				else if(split_command[0] == "exit")
				{
					client_logout();
        			close (STDOUT_FILENO);
        			close (STDERR_FILENO);
					shmdt(Client_set);
					exit(0);
				}
		/*---------------------client command--------------------------------------*/
				else if(split_command[0] == "who")
				{
					who();
					split_command.clear();
					continue;
				}
				else if(split_command[0] == "tell")
				{
					int tell_id_size = split_command[1].size();
					int tell_id = stoi(split_command[1]);
					string tell_string = Command.erase(0,5+tell_id_size+1); // tell 3 msg
					//cout << "test tell string : " << tell_string << endl; 
					tell(tell_id, tell_string);
					split_command.clear();
					continue;
				}
				else if(split_command[0] == "yell")
				{
					string yell_string = Command.erase(0,5);
					//cout << "test yell string : " << yell_string << endl; 
					yell(yell_string);
					split_command.clear();
					continue;
				}
				else if(split_command[0] == "name")
				{
					name(client_ipaddr, client_port,  split_command[1]);
					split_command.clear();
					continue;
				}				
				
		/* ---------------------------command---------------------------------------*/			
				while(split_command.size() != 0)
				{
					Com_content command_content;
					initialize_command(command_content);
					//cout << "command_FIFO_type: " << command_content.FIFO_type << endl;
					command_content.command = split_command[0];
					//cout << " command_content : " << command_content.command << endl;
					split_command.erase(split_command.begin());
					
					//last command 
					if(split_command.size() == 0)
					{
						command_content.pipe_to_num = -1;
						command_content.pipe_type = no_pipe_exclamtion;
						vec_command.push_back(command_content);
						break;
					}
					
					//cout << "split_command: " << split_command[0] << endl;
					/*-------------------------------store parameter--------------------------------*/
					while(split_command[0][0] != '|' && split_command[0][0] != '!' && split_command[0][0] != '>' && split_command[0][0] != '<') 
					{ 
						command_content.parameter.push_back(split_command[0]);
						split_command.erase(split_command.begin());		

						if(split_command.size() == 0)   //last command 
						{
							command_content.pipe_to_num = -1;
							command_content.pipe_type = no_pipe_exclamtion;
							vec_command.push_back(command_content);
							break;
						}
					}
					
					/*-------------------------------if it is not parameter and it is not the last command--------------------------------*/
					if(split_command.size() != 0)
					{
						if(split_command[0][0] == '|')  // pipe 
						{
							command_content.FIFO_to_user = -1;
							command_content.FIFO_from_user = -1;
							command_content.FIFO_type = -1;
							if(split_command[0].size() == 1)   //only "|"
							{
								command_content.pipe_to_num = 1;
								command_content.pipe_type = include_pipe;
								vec_command.push_back(command_content);
							}
							else   // have number "|5"
							{
								split_command[0].erase(0,1);   //erase "|" or "!"
								command_content.pipe_to_num = stoi(split_command[0]);
								command_content.pipe_type = include_pipe;
								vec_command.push_back(command_content);
							}
							split_command.erase(split_command.begin());
						}
						else if(split_command[0][0] == '!')
						{
							command_content.FIFO_to_user = -1;
							command_content.FIFO_from_user = -1;
							command_content.FIFO_type = -1;
							if(split_command[0].size() == 1)   //only "!"
							{
								command_content.pipe_to_num = 1;
								command_content.pipe_type = include_exclamation;
								vec_command.push_back(command_content);
							}
							else   // have number "!5"
							{
								split_command[0].erase(0,1);   //erase "!"
								command_content.pipe_to_num = stoi(split_command[0]);
								command_content.pipe_type = include_exclamation;
								vec_command.push_back(command_content);
							}
							split_command.erase(split_command.begin());		//erase "!"	or "!5"	
						}
						else if(split_command[0][0] == '>')
						{
							if(split_command[0].size() == 1)   //only ">"
							{
								//export
								command_content.FIFO_to_user = -1;
								command_content.FIFO_from_user = -1;
								command_content.FIFO_type = -1;
								command_content.pipe_to_num = -1;
								command_content.pipe_type = include_input_text;
								split_command.erase(split_command.begin());
								command_content.parameter.push_back(split_command[0]);  // get the text file
								vec_command.push_back(command_content);
								split_command.erase(split_command.begin());	
							}
							else //  with FIFO  ex: >2 , would not have to pipe to others 
							{
								split_command[0].erase(0,1);   //erase ">"
								command_content.FIFO_to_user = stoi(split_command[0]);
								split_command.erase(split_command.begin()); //erase number;
								command_content.original_command	= Command;
								
								if(split_command[0][0] == '<')  // have FIFO "<" in behind, ex: <5
								{
									//need to read FIFO
									split_command[0].erase(0,1);   //erase "<"
									command_content.FIFO_type = both_read_and_write_FIFO;
									command_content.FIFO_from_user = stoi(split_command[0]);
									command_content.pipe_to_num = -1;
									command_content.pipe_type = no_pipe_exclamtion;
									vec_command.push_back(command_content);
									split_command.erase(split_command.begin());	
								}
								else
								{
									//don't  need to FIFO from others
									command_content.FIFO_type = only_write_to_FIFO;
									command_content.FIFO_from_user = -1;
									command_content.pipe_to_num = -1;
									command_content.pipe_type = no_pipe_exclamtion;
									vec_command.push_back(command_content);									
								}			
							}
						}
						else if(split_command[0][0] == '<')     //FIFO from other
						{
							split_command[0].erase(0,1);   //erase "<"
							command_content.FIFO_from_user = stoi(split_command[0]);
							split_command.erase(split_command.begin()); //erase number;		
							command_content.original_command	= Command;

								if(split_command[0][0] == '|' || split_command[0][0] == '!' || split_command[0][0] == '>')
								{
									// have FIFO "｜ ,  !  ,  >  ,  >5" in behind . ex: cat <2 >1
									if(split_command[0][0] == '|')
									{
										//don't need to  write FIFO 
										// but need to pipe to others
										command_content.FIFO_to_user = -1;
										command_content.FIFO_type = only_read_from_FIFO;
										command_content.pipe_type = include_pipe;
										if(split_command[0].size() == 1)   //only "|"
										{
											command_content.pipe_to_num = 1;
										}
										else   // have number "|5"
										{
											split_command[0].erase(0,1);   //erase "|" or "!"
											command_content.pipe_to_num = stoi(split_command[0]);
										}
										vec_command.push_back(command_content);
										split_command.erase(split_command.begin());										
									}
									else if(split_command[0][0] == '!')
									{
										command_content.FIFO_to_user = -1;
										command_content.FIFO_type = only_read_from_FIFO;
										command_content.pipe_type = include_exclamation;
										if(split_command[0].size() == 1)   //only "!"
										{
											command_content.pipe_to_num = 1;
										}
										else   // have number "!5"
										{
											split_command[0].erase(0,1);   //erase "!"
											command_content.pipe_to_num = stoi(split_command[0]);
										}
										vec_command.push_back(command_content);
										split_command.erase(split_command.begin());	
									}
									else if(split_command[0][0] == '>')
									{
										if(split_command[0].size() == 1)   //only ">", ex:     (cat <1) > a.txt   
										{
											//export to file
											command_content.FIFO_to_user = -1;
											command_content.FIFO_type = only_read_from_FIFO;
											command_content.pipe_to_num = -1;
											command_content.pipe_type = include_input_text;
											split_command.erase(split_command.begin());  //erase ">"
											command_content.parameter.push_back(split_command[0]);  // get the text file
											split_command.erase(split_command.begin());	//erase text file
										}
										else //  with FIFO  ex:   (cat <1)    >2 
										{
											//FIFO to other users
											split_command[0].erase(0,1);   //erase ">"
											command_content.FIFO_to_user = stoi(split_command[0]);
											command_content.FIFO_type = both_read_and_write_FIFO;
											command_content.pipe_to_num = -1;
											command_content.pipe_type = no_pipe_exclamtion;		
											split_command.erase(split_command.begin()); //erase number, size will be 0
										}		
										vec_command.push_back(command_content);
									}	
								}
								else // don't have any "| ! >" behing  ex:   cat <2 or cat <2 cat <1
								{
									// don't need to  write FIFO and pipe to others
									command_content.FIFO_type = only_read_from_FIFO;
									command_content.FIFO_to_user = -1;
									command_content.pipe_to_num = -1;
									command_content.pipe_type = no_pipe_exclamtion;
									vec_command.push_back(command_content);							
								}	
						}			
					}
				}
		
			/* ------------------------------------pipeN---------------------------------------------*/		
					int PipeN_number = vec_command[vec_command.size()-1].pipe_to_num;
						
					pid_t last_pid;
			/* ------------------------------------command execute-----------------------------------*/
					for(int i=0;i<vec_command.size();i++)
					{
						int same_pipe_table_number = -1;
			/* ------------------------------------check pipe table counter-----------------------------------*/
						if(vec_command[i].pipe_type == include_pipe || vec_command[i].pipe_type == include_exclamation)
						{
							// check the prior counter and compare with the pipe to number
							// if equal, use the prior pipe instead of creating a new pipe
							for(int j=0;j<vec_pipe_table.size();j++)
							{
								if(vec_pipe_table[j].counter == vec_command[i].pipe_to_num) 
								{
									same_pipe_table_number = j;
									args_execute(vec_command[i], vec_pipe_table, same_pipe_table_number, last_pid);
									break;
								}
							}
							if(same_pipe_table_number == -1)
							{
							/*-------------------------------------create pipe----------------------------------------*/				
								int pipefd[2];
								if (pipe(pipefd) < 0) 
									exit(1); 
							
								Pipe_table pipe_table;			
					
								pipe_table.fd_in = pipefd[1];
								pipe_table.fd_out = pipefd[0];
								pipe_table.counter = vec_command[i].pipe_to_num;
							
								vec_pipe_table.push_back(pipe_table);		
								
								args_execute(vec_command[i], vec_pipe_table, same_pipe_table_number, last_pid);
							}
							
						}  
						else if(vec_command[i].pipe_type == no_pipe_exclamtion || vec_command[i].pipe_type == include_input_text)  //last command, no pipe or exclamation, don't need to create pipe
						{
							args_execute(vec_command[i], vec_pipe_table, same_pipe_table_number,last_pid);
						}
					
						for(int j=0;j<vec_pipe_table.size();j++)
						{
							if(vec_pipe_table[j].counter == 0)
							{	
								close(vec_pipe_table[j].fd_in);
								close(vec_pipe_table[j].fd_out);
								vec_pipe_table.erase(vec_pipe_table.begin()+j);
							}
							vec_pipe_table[j].counter -= 1;
						}			
						
					}

						
					if(PipeN_number == -1)
					{
						int status;
						waitpid(last_pid, &status, 0);
					}
					vec_command.clear();
			
		}
	}
}

vector<string>split_space(string command)
{
	istringstream split_s(command);
    vector<string> split_command((istream_iterator<string>(split_s)),
                                istream_iterator<string>());
								
    return split_command;
}

void initialize_command(Com_content &command_content)
{
	command_content.original_command.clear();
	command_content.command.clear();
	command_content.parameter.clear();
	command_content.pipe_to_num = -1;
	command_content.pipe_type = no_pipe_exclamtion;
	command_content.FIFO_type = -1;
	command_content.FIFO_from_user = -1;
	command_content.FIFO_to_user = -1;
}

void args_execute(Com_content command_content, vector<Pipe_table> vec_pipe_table, int same_pipe_table_number, pid_t &last_pid)
{
	//cout << "args_execute" << endl;
	signal(SIGCHLD, childHandler);
    int sender_open_fifo = -1;
	bool error_happen = FIFO_operation(command_content, sender_open_fifo);
	if(error_happen == true)
	{ 
        return;   
    }
	//cout << "sender open fifo: " << sender_open_fifo << endl;
    pid_t pid;
	while((pid = fork()) < 0) 
	{
		usleep (1000);
	}
	last_pid = pid;
		
	if(pid == 0)   // child process
	{

		//give my stdout to pipe
		if(command_content.pipe_to_num != -1) //   there is a "|" or "!" back of me
		{
			if(same_pipe_table_number != -1)  // used prior table
			{
				if(command_content.pipe_type == include_pipe)
				{
					close(vec_pipe_table[same_pipe_table_number].fd_out);
					dup2(vec_pipe_table[same_pipe_table_number].fd_in,STDOUT_FILENO);	
					close(vec_pipe_table[same_pipe_table_number].fd_in);
				}
				else if(command_content.pipe_type == include_exclamation)
				{
					close(vec_pipe_table[same_pipe_table_number].fd_out);
					dup2(vec_pipe_table[same_pipe_table_number].fd_in,STDOUT_FILENO);	
					dup2(vec_pipe_table[same_pipe_table_number].fd_in,STDERR_FILENO);
					close(vec_pipe_table[same_pipe_table_number].fd_in);							
				}
			}	
			else  // used new table
			{
				if(command_content.pipe_type == include_pipe)
				{	
					close(vec_pipe_table[vec_pipe_table.size()-1].fd_out);
					dup2(vec_pipe_table[vec_pipe_table.size()-1].fd_in,STDOUT_FILENO);	
					close(vec_pipe_table[vec_pipe_table.size()-1].fd_in);
				}
				else if(command_content.pipe_type == include_exclamation)
				{
					close(vec_pipe_table[vec_pipe_table.size()-1].fd_out);
					dup2(vec_pipe_table[vec_pipe_table.size()-1].fd_in,STDOUT_FILENO);
					dup2(vec_pipe_table[vec_pipe_table.size()-1].fd_in,STDERR_FILENO);
					close(vec_pipe_table[vec_pipe_table.size()-1].fd_in);							
				}
			}
		}
		// for ">"
		if(command_content.pipe_type == include_input_text)
		{
			int size = command_content.parameter.size();
			string file_name = command_content.parameter[size-1];
			command_content.parameter.pop_back();
			int openfile = open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
			dup2(openfile,STDOUT_FILENO);	
			close(openfile);
		}
			
		//get prior pipetable && check if prior pipetable pipe to me
		for(int j=0;j<vec_pipe_table.size();j++)
		{
			if(vec_pipe_table[j].counter == 0)  //someone want to give me pipe
			{	
				close(vec_pipe_table[j].fd_in);
				dup2(vec_pipe_table[j].fd_out,STDIN_FILENO);
				close(vec_pipe_table[j].fd_out);
			}
			
		}	

        if(command_content.FIFO_type == only_write_to_FIFO)
        {
            //write
	        dup2(sender_open_fifo,STDOUT_FILENO);	
			close(sender_open_fifo);
        }
        else if(command_content.FIFO_type == only_read_from_FIFO)
        {
            int sender_id = command_content.FIFO_from_user;
            int receiver_id = uid;
			//read the FIFO
            int receiver_open_fifo = Client_set[receiver_id].FIFO_fd_table[sender_id];
			//cout << "receiver open fifo : " << receiver_open_fifo << endl;
			dup2(receiver_open_fifo, STDIN_FILENO);
			close(receiver_open_fifo);
            //after read => delete FIFO file
			string fifo_pathname =  "user_pipe/" + to_string(sender_id) + "_" + to_string(receiver_id);
            delete_fifo(fifo_pathname);
        }
        else if(command_content.FIFO_type == both_read_and_write_FIFO)
        {
            int sender_id = command_content.FIFO_from_user;
            int receiver_id = uid;
            int receiver_open_fifo = Client_set[receiver_id].FIFO_fd_table[sender_id];
            //read the FIFO
			dup2(receiver_open_fifo, STDIN_FILENO);
			close(receiver_open_fifo);

            //delete FIFO file            
            string fifo_pathname =  "user_pipe/" + to_string(sender_id) + "_" + to_string(receiver_id);
            delete_fifo(fifo_pathname);
            
            //write
	        dup2(sender_open_fifo,STDOUT_FILENO);	
			close(sender_open_fifo);
        }
        
			//execute
			char *argv[command_content.parameter.size() + 2] = {};
			argv[0] = strdup(command_content.command.c_str());
			argv[command_content.parameter.size() + 1] = (char*)NULL;
			for(int i=0;i<command_content.parameter.size();i++)
			{
				argv[i+1] = strdup(command_content.parameter[i].c_str());
			}		
				
			int result = execvp(command_content.command.c_str(), argv);
			if (result == -1)
			{
				cerr << "Unknown command: [" << command_content.command << "]." << endl;
				exit(0); 
			}
		}
	else if(pid > 0)
	{
		//parent
		string fifo_command = command_content.original_command;
        if(command_content.FIFO_type == only_write_to_FIFO)
        {        
            close(sender_open_fifo);
        }
        else if(command_content.FIFO_type == only_read_from_FIFO)
        {			
            //read the FIFO
            int sender_id = command_content.FIFO_from_user;
            int receiver_id = uid;
            int receiver_open_fifo = Client_set[receiver_id].FIFO_fd_table[sender_id];
			close(receiver_open_fifo);
        }
        else if(command_content.FIFO_type == both_read_and_write_FIFO)
        {
            char msg[MAX_MSG_SIZE + 1];
            //read
            int sender_id = command_content.FIFO_from_user;
            int receiver_id = uid;
            int receiver_open_fifo = Client_set[receiver_id].FIFO_fd_table[sender_id];
			close(receiver_open_fifo);
            close(sender_open_fifo);
        }

	}
	else
	{ 
		//error
	}
}

void childHandler(int signo) {
    int status;
    pid_t pid ;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        //do nothing
    }
}


void initialize_shared_mem()
{
    //get the shared memory for client
     if ((shmid = shmget (SHMKEY, MAX_CLIENT * sizeof (Client), 0666)) < 0) 
	 {
        perror("Shmget Error");
        exit (1);
    }
    // attach the allocated shared memory
    if (  (Client_set = (Client *) shmat (shmid, nullptr, 0) ) == (Client *) -1) 
	{
        perror("Shmat Error");
        exit (1);
    }
}

void initialize_semaphore()
{
    // Client get Semaphore
    if ((semid =  semget((key_t)SEMKEY, 1, 0666)) < 0)
	{
        perror("Semget Error");
        exit(1);
    }	
	//cout << semid << endl;
}

void client_login(char *client_ipaddr, int client_port)
{
	//cout << "login" << endl;
	char	msg[MAX_MSG_SIZE + 1];
	for(int i=1;i<MAX_CLIENT;i++)   // 1~30
	{
		
		// Access Semaphore
		if(access_semaphore() == false)
		{
			cerr << "login in semaphore fail" << endl;
			cerr << "access semaphore fail" << endl;
		}
		
		if(Client_set[i].used == false)
		{
			Client_set[i].used = true;
			Client_set[i].id = i; 
			Client_set[i].pid = getpid();
			Client_set[i].port = client_port;
			for(int j=1;j<MAX_CLIENT;j++)
			{
				Client_set[i].FIFO_fd_table[j] = -1;
			}
			char client_name[] = "(no name)";
			strncpy(Client_set[i].name, client_name, sizeof(Client_set[i].name));
			strncpy(Client_set[i].addr, client_ipaddr, sizeof(Client_set[i].addr));

            snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' entered from %s:%d. ***\n",
                    Client_set[i].name, Client_set[i].addr, Client_set[i].port);
            //string print_msg = "*** User '" + string(Client_set[i].name) +  "' entered from " + string(Client_set[i].addr) + "/" + to_string(Client_set[i].port) +". ***";
			uid = i;
			
			// Release Semaphore
			if(release_semaphore() == false)
			{
				cerr << "release semaphore fail" << endl;
			}
			
			broadcast(msg);
			break;
		}
		// Release Semaphore
		if(release_semaphore() == false)
		{
			cerr << "release semaphore fail" << endl;
		}
	}
}


void client_logout()
{
	char	msg[MAX_MSG_SIZE + 1];
	
	// Access Semaphore
	if(access_semaphore() == false)
	{
		cerr << "In Client logout semaphore" << endl;
		cerr << "access semaphore fail" << endl;
	}
	
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' left. ***\n",Client_set[uid].name);
	
	// Release Semaphore
	if(release_semaphore() == false)
	{
		cerr << "release semaphore fail" << endl;
	}
	
	broadcast(msg);
	
	// Access Semaphore
	if(access_semaphore() == false)
	{
		cerr << "In Client logout semaphore" << endl;
		cerr << "access semaphore fail" << endl;
	}
	
	// clear all FIFO file I write or read
	for(int i=1;i<MAX_CLIENT;i++)
	{
		//cout << "checking" << endl;
		string fifo_pathname_I_write_before =  "user_pipe/" + to_string(uid) + "_" + to_string(i);
		if(access(fifo_pathname_I_write_before.c_str(),F_OK) == 0)
		{
			//file exist
			delete_fifo(fifo_pathname_I_write_before);
		}
		
		string fifo_pathname_I_need_to_read =  "user_pipe/" + to_string(i) + "_" + to_string(uid);
		if(access(fifo_pathname_I_need_to_read.c_str(),F_OK) == 0)
		{
			//file exist
			delete_fifo(fifo_pathname_I_need_to_read);
		}
	}
	// clear Client information
	memset (&Client_set[uid], 0, sizeof(Client));
	
	// Release Semaphore
	if(release_semaphore() == false)
	{
		cerr << "release semaphore fail" << endl;
	}

}
void broadcast(char *msg)
{
	// Access Semaphore
	if(access_semaphore() == false)
	{
		cerr << "In client Handler" << endl;
		cerr << "access semaphore fail" << endl;
	}
	for(int i=1; i<MAX_CLIENT; i++)
	{
		//for exist client
		if(Client_set[i].used == 1)
		{	
			strncpy(Client_set[i].msg, msg, MAX_MSG_SIZE+1);
			
			// singal 
			kill (Client_set[i].pid, SIGUSR1);
		}
	}
	// Release Semaphore
	if(release_semaphore() == false)
	{
		cerr << "release semaphore fail" << endl;
	} 
}


void clientHandler(int signo)
{
	if(signo == SIGUSR1)
	{		
        if (Client_set[uid].msg != 0) 
		{
			//print msg I get
    		write (STDOUT_FILENO, Client_set[uid].msg, strlen(Client_set[uid].msg));
             // clear msg buffer
            memset (Client_set[uid].msg, 0, MAX_MSG_SIZE);
		}
	}
	else if (signo == SIGINT || signo == SIGQUIT || signo == SIGTERM) 
	{
        	//client_logout();
        	close (STDOUT_FILENO);
        	close (STDERR_FILENO);
			shmdt(Client_set);
			exit(0);
    }	
		
}

void who()
{
	// Access Semaphore
	if(access_semaphore() == false)
	{
		cerr << "access semaphore fail" << endl;
	}	
	cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
	for(int i=1;i< MAX_CLIENT;i++)
	{
		if(Client_set[i].used == true)
		{
			if(uid == Client_set[i].id)
				cout << Client_set[i].id << "\t"<< Client_set[i].name << "\t" 
				<< Client_set[i].addr << ":" << Client_set[i].port << "\t<-me" << endl;
			else
				cout << Client_set[i].id << "\t"<< Client_set[i].name << "\t" 
				<< Client_set[i].addr << ":" << Client_set[i].port << endl;	
		}
	}
	// Release Semaphore
	if(release_semaphore() == false)
	{
		cerr << "release semaphore fail" << endl;
	}	
}


void tell(int tell_id, string tell_string)
{
	char msg[MAX_MSG_SIZE + 1];
	
	// Access Semaphore
	if(access_semaphore() == false)
	{
		cerr << "access semaphore fail" << endl;
	}	
	if(Client_set[tell_id].used == false)
	{	
		cout << "*** Error: user #" << tell_id  << " does not exist yet. ***" << endl;
		// Release Semaphore
		if(release_semaphore() == false)
		{
			cerr << "release semaphore fail" << endl;
		}	
	}
	else
	{
		// user exist
		snprintf (msg, MAX_MSG_SIZE + 1,"*** %s told you ***: %s\n",  Client_set[uid].name, tell_string.c_str());	
		strncpy(Client_set[tell_id].msg, msg, MAX_MSG_SIZE+1);
		
		// Release Semaphore
		if(release_semaphore() == false)
		{
			cerr << "release semaphore fail" << endl;
		}	
		kill (Client_set[tell_id].pid, SIGUSR1);		
	}
}

void  yell(string yell_string)
{
	char msg[MAX_MSG_SIZE + 1];
	
	// Access Semaphore
	if(access_semaphore() == false)
	{
		cerr << "access semaphore fail" << endl;
	}
	
    snprintf (msg, MAX_MSG_SIZE + 1, "*** %s yelled ***: %s\n",  Client_set[uid].name, yell_string.c_str());
	
	// Release Semaphore
	if(release_semaphore() == false)
	{
		cerr << "release semaphore fail" << endl;
	}
	
	broadcast(msg);
}

void name(char *client_ipaddr, int client_port, string client_new_name)
{
	bool name_exist = false;
	char msg[MAX_MSG_SIZE + 1];
	// Access Semaphore
	if(access_semaphore() == false)
	{
		cerr << "access semaphore fail" << endl;
	}

	for(int i=1;i<MAX_CLIENT;i++)
	{
		if(strcmp(Client_set[i].name, strdup(client_new_name.c_str())) == 0)
		{
			//if other user used the name
			cout << "*** User '"+ client_new_name + "' already exists. ***" << endl;
			name_exist = true;
			// Release Semaphore
			if(release_semaphore() == false)
			{
				cerr << "release semaphore fail" << endl;
			}	
			break;
		}
	}

	if(name_exist == false)
	{
		strncpy(Client_set[uid].name, strdup(client_new_name.c_str()), sizeof(Client_set[uid].name));
        snprintf (msg, MAX_MSG_SIZE + 1, "*** User from %s:%d is named '%s'. ***\n", 
		client_ipaddr, client_port, Client_set[uid].name);
      	// Release Semaphore
		if(release_semaphore() == false)
		{
			cerr << "release semaphore fail" << endl;
		}	    

		broadcast(msg);
	}
}


bool access_semaphore()
{
	struct sembuf sem_buffer;
	sem_buffer.sem_num =0; //first semaphore
	sem_buffer.sem_op = -1;   //lock
	sem_buffer.sem_flg = SEM_UNDO;
	if(TEMP_FAILURE_RETRY(semop(semid, &sem_buffer,1)) < 0)
	{
		//cout << semid << endl;
        cerr << "Access semaphore failed" << endl;
        return false;		
	}
	//cout << "lock" << endl;
	return true;
}

bool release_semaphore()
{
	struct sembuf sem_buffer;
	sem_buffer.sem_num =0; //first semaphore
	sem_buffer.sem_op = 1;   //unlock
	sem_buffer.sem_flg = SEM_UNDO;
	if(TEMP_FAILURE_RETRY(semop(semid, &sem_buffer,1)) < 0)
	{
        cerr << "Release semaphore failed\n" << endl;
        return false;		
	}
	//cout << "unlock" << endl;
	return true;	
}


bool FIFO_operation(Com_content command_content, int &sender_open_fifo)
{
	// for FIFO
	if(command_content.FIFO_type != -1)
	{
		//cout << "FIFO type: " << command_content.FIFO_type << endl;
		if(command_content.FIFO_type == only_read_from_FIFO)
		{
			string fifo_command = command_content.original_command;
			bool error_happen = get_msg_from_fifo(command_content.FIFO_from_user, uid, fifo_command); // sender , receiver, msg
			return error_happen;
		}
		else if(command_content.FIFO_type == only_write_to_FIFO)
		{
			string fifo_command = command_content.original_command;
			bool error_happen = write_msg_to_fifo(uid, command_content.FIFO_to_user, fifo_command, sender_open_fifo);
			return error_happen;
		}
		else if(command_content.FIFO_type == both_read_and_write_FIFO)
		{
			string fifo_command = command_content.original_command;
			//READ
			bool  error_happen = get_msg_from_fifo(command_content.FIFO_from_user, uid, fifo_command);    //sender , receiver 
			if(error_happen == true)
				return true;
			else
			{
				//WRITE
				bool error_happen_w = write_msg_to_fifo(uid, command_content.FIFO_to_user, fifo_command, sender_open_fifo);  // sender, receiver
				return error_happen_w;		
			}
		}
	}
	return false;
}

bool write_msg_to_fifo(int sender_id, int receiver_id, string fifo_command, int &sender_open_fifo)
{
	char msg[MAX_MSG_SIZE + 1];
	bool receiver_exist = true;
	//check whether the user exist
	
	// Access Semaphore
	if(access_semaphore() == false)
	{
		cerr << "access semaphore fail" << endl;
	}
	if(Client_set[receiver_id].used == false)
	{	
		cout << "*** Error: user #" << receiver_id  << " does not exist yet. ***" << endl;
		receiver_exist = false;
	}
	// Release Semaphore
	if(release_semaphore() == false)
	{
		cerr << "release semaphore fail" << endl;
	}

	if(receiver_exist == true)
	{
		string fifo_pathname =  "user_pipe/" + to_string(sender_id) + "_" + to_string(receiver_id);

		if(access(fifo_pathname.c_str(),F_OK) == -1)
		{
			//no file exist
			create_fifo(fifo_pathname);
			//cout << "created fifo" << endl;
			
			// Access Semaphore
			if(access_semaphore() == false)
			{
				cerr << "access semaphore fail" << endl;
			}	

			// store sender_id
			Client_set[receiver_id].open_FIFO_with_id = sender_id;
			
			// Release Semaphore
			if(release_semaphore() == false)
			{
				cerr << "release semaphore fail" << endl;
			}
			
			//signal other user to open FIFO
			kill (Client_set[receiver_id].pid, SIGUSR2);
			
			//cout << "signal other" << endl;
			
			//open FIFO
			sender_open_fifo = open(fifo_pathname.c_str(), O_WRONLY);
			if(sender_open_fifo < 0)
			{
				cerr << "Open FIFO File Error" << endl;
				exit(1);			
			}

			//broadcast msg
	        snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n" ,  
		    Client_set[sender_id].name ,sender_id, fifo_command.c_str(), Client_set[receiver_id].name, receiver_id);
		    broadcast(msg);
			//cout << "sender open FIFO: " << sender_open_fifo << endl;
			//cout << "sender open fifo_pathname: " << fifo_pathname << endl;
			return false;
		}
		else
		{
			//file exist
			cout << "*** Error: the pipe #" <<  sender_id << "->#" << receiver_id << " already exists. ***" << endl;
			return true; //error happen
		}
	}
	else
		return true; // error happen
}

void create_fifo(string pathname)
{
	int FIFO_file = mkfifo(pathname.c_str(),0777);
	if(FIFO_file < 0)
	{
		cerr << "Create FIFO File Error" << endl;
		exit(1);
	}
}

bool get_msg_from_fifo(int sender_id, int receiver_id, string fifo_command)
{
	char msg[MAX_MSG_SIZE + 1];
	bool sender_exist = true;
	//check whether the user exist
	// Access Semaphore
	if(access_semaphore() == false)
	{
		cerr << "access semaphore fail" << endl;
	}	
	if(Client_set[sender_id].used == false)
	{	
		cout << "*** Error: user #" << sender_id  << " does not exist yet. ***" << endl;
		sender_exist = false;
	}
	// Release Semaphore
	if(release_semaphore() == false)
	{
		cerr << "release semaphore fail" << endl;
	}

	if(sender_exist == true)
	{
        string fifo_pathname =  "user_pipe/" + to_string(sender_id) + "_" + to_string(receiver_id);
		if(access(fifo_pathname.c_str(),F_OK) != -1)
		{
			//broadcast msg
			snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n" ,  
			Client_set[receiver_id].name ,receiver_id,  Client_set[sender_id].name, sender_id, fifo_command.c_str());
			broadcast(msg);
			return false; // error did not happen
		}
		else
		{
			//fifo file is not exist
			cout << "*** Error: the pipe #" << sender_id << "->#" << receiver_id << " does not exist yet. ***" << endl;
			return true; // error happen	
		}
	}
	else
		return true; // error happen
}


void delete_fifo(string fifo_pathname)
{
	int delete_fifo_file = unlink(fifo_pathname.c_str());
	if(delete_fifo_file < 0)
	{
		cerr << "Delete FIFO File Error" << endl;
		exit(1);		
	}	
}

void FIFO_sigHandler(int signo)
{
	if(signo == SIGUSR2)
	{	
		// Access Semaphore
		if(access_semaphore() == false)
		{
			cerr << "access semaphore fail" << endl;
		}
		
		//cout << "receiver get signal" << endl;
		int sender_id = Client_set[uid].open_FIFO_with_id;
		//cout << "sender_id : " << sender_id << endl;
		string fifo_pathname =  "user_pipe/" + to_string(sender_id) + "_" + to_string(uid) ;
		int receiver_open_fifo = open(fifo_pathname.c_str(), O_RDONLY);
		if(receiver_open_fifo < 0)
		{
			cerr << "Receiver open FIFO Failed" << endl;
			exit(1);
		}
		Client_set[uid].FIFO_fd_table[sender_id] = receiver_open_fifo;
		
		// Release Semaphore
		if(release_semaphore() == false)
		{
			cerr << "release semaphore fail" << endl;
		}
	}
}
