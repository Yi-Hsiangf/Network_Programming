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
#include<map>

#include <sys/types.h>
#include <sys/stat.h>


#include "np2_shell.h"

using namespace std;


static void clientHandler(int signo);


// pipe type
const int no_pipe_exclamtion = 0;
const int include_pipe = 1;
const int include_exclamation = 2;
const int include_input_text = 3;
// FIFO type
const int only_write_to_user_pipe = 4;
const int only_read_from_user_pipe = 5;
const int both_read_and_write_user_pipe = 6;


bool NPShell(int sockfd, char* buffer, client &Client, vector<client>&Client_set)
{ 
	//string PATH = "bin:.";
	vector<Com_content> vec_command;
	vector<string> split_command;

	int uid = Client.id;
	set_client_env(uid, Client_set);

	string Command = string(buffer);
	//cout << Command.size() << endl;
	int size = Command.size()-1;
	for(int i = size; i >= 0;i--)
	{
		if(Command[i] == '\r' || Command[i] == '\n')
		Command.erase(Command.begin()+i);
	}

	if(Command.size() == 0)
		return false;
	split_command = split_space(Command);

/* ------------------------printenv,setenv-------------------------------*/		
	if(split_command[0] == "printenv")
	{
		char *env = getenv(split_command[1].c_str());
		if(env == NULL)
		{
			return false;
		}
		cout << env << endl;
		split_command.clear();
		return false;
	}
	else if(split_command[0] == "setenv")
	{
		//map_variable_to_path[split_command[1]] = split_command[2];
		setenv(split_command[1].c_str(), split_command[2].c_str(), 1);
		Client_set[uid].client_env_map[split_command[1]] = split_command[2];
		split_command.clear();
		return false;
	}
	else if(split_command[0] == "exit")
	{
		return true;
	}
/*---------------------client command--------------------------------------*/
	else if(split_command[0] == "who")
	{
		who(uid, Client_set);
		split_command.clear();
		return false;
	}
	else if(split_command[0] == "tell")
	{
		int tell_id_size = split_command[1].size();
		int tell_id = stoi(split_command[1]);
		string tell_string = Command.erase(0,5+tell_id_size+1); // tell 3 msg
		//cout << "test tell string : " << tell_string << endl; 
		tell(uid, tell_id, tell_string, Client_set);
		split_command.clear();
		return false;
	}
	else if(split_command[0] == "yell")
	{
		string yell_string = Command.erase(0,5);
		//cout << "test yell string : " << yell_string << endl; 
		yell(uid, yell_string, Client_set);
		split_command.clear();
		return false;
	}
	
	else if(split_command[0] == "name")
	{
		name(uid, split_command[1], Client_set);
		split_command.clear();
		return false;
	}
	
	
/* ---------------------------command---------------------------------------*/			
	while(split_command.size() != 0)
	{
		Com_content command_content;
		initialize_command(command_content);
		//cout << "command_user_pipe_type: " << command_content.user_pipe_type << endl;
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
				command_content.pipe_to_user = -1;
				command_content.pipe_from_user = -1;
				command_content.user_pipe_type = -1;
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
				command_content.pipe_to_user = -1;
				command_content.pipe_from_user = -1;
				command_content.user_pipe_type = -1;
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
					command_content.pipe_to_user = -1;
					command_content.pipe_from_user = -1;
					command_content.user_pipe_type = -1;
					command_content.pipe_to_num = -1;
					command_content.pipe_type = include_input_text;
					split_command.erase(split_command.begin());
					command_content.parameter.push_back(split_command[0]);  // get the text file
					vec_command.push_back(command_content);
					split_command.erase(split_command.begin());	
				}
				else //  with pipe  ex: >2 , would not have to pipe to others 
				{
					split_command[0].erase(0,1);   //erase ">"
					command_content.pipe_to_user = stoi(split_command[0]);
					split_command.erase(split_command.begin()); //erase number;
					command_content.original_command	= Command;
					
					if(split_command[0][0] == '<')  // have pipe "<" in behind, ex: <5
					{
						//need to read user pipe
						split_command[0].erase(0,1);   //erase "<"
						command_content.user_pipe_type = both_read_and_write_user_pipe;
						command_content.pipe_from_user = stoi(split_command[0]);
						command_content.pipe_to_num = -1;
						command_content.pipe_type = no_pipe_exclamtion;
						vec_command.push_back(command_content);
						split_command.erase(split_command.begin());	
					}
					else
					{
						//don't  need to pipe from others
						command_content.user_pipe_type = only_write_to_user_pipe;
						command_content.pipe_from_user = -1;
						command_content.pipe_to_num = -1;
						command_content.pipe_type = no_pipe_exclamtion;
						vec_command.push_back(command_content);									
					}			
				}
			}
			else if(split_command[0][0] == '<')     //pipe from other
			{
				split_command[0].erase(0,1);   //erase "<"
				command_content.pipe_from_user = stoi(split_command[0]);
				split_command.erase(split_command.begin()); //erase number;		
				command_content.original_command	= Command;

					if(split_command[0][0] == '|' || split_command[0][0] == '!' || split_command[0][0] == '>')
					{
						// have FIFO "｜ ,  !  ,  >  ,  >5" in behind . ex: cat <2 >1
						if(split_command[0][0] == '|')
						{
							//don't need to  write to user pipe 
							// but need to pipe to others
							command_content.pipe_to_user = -1;
							command_content.user_pipe_type = only_read_from_user_pipe;
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
							command_content.pipe_to_user = -1;
							command_content.user_pipe_type = only_read_from_user_pipe;
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
								command_content.pipe_to_user = -1;
								command_content.user_pipe_type = only_read_from_user_pipe;
								command_content.pipe_to_num = -1;
								command_content.pipe_type = include_input_text;
								split_command.erase(split_command.begin());  //erase ">"
								command_content.parameter.push_back(split_command[0]);  // get the text file
								split_command.erase(split_command.begin());	//erase text file
							}
							else //  with user pipe  ex:   (cat <1)    >2 
							{
								//pipe to other users
								split_command[0].erase(0,1);   //erase ">"
								command_content.pipe_to_user = stoi(split_command[0]);
								command_content.user_pipe_type = both_read_and_write_user_pipe;
								command_content.pipe_to_num = -1;
								command_content.pipe_type = no_pipe_exclamtion;		
								split_command.erase(split_command.begin()); //erase number, size will be 0
							}		
							vec_command.push_back(command_content);
						}			
					}
					else //  ex:  cat <2,      cat <2 cat <1
					{
						// don't need to  write user_pipe and pipe to others 
						command_content.user_pipe_type = only_read_from_user_pipe;
						command_content.pipe_to_user = -1;
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
				for(int j=0;j<Client_set[uid].vec_pipe_table.size();j++)
				{
					if(Client_set[uid].vec_pipe_table[j].counter == vec_command[i].pipe_to_num) 
					{
						same_pipe_table_number = j;
						args_execute(vec_command[i], same_pipe_table_number, last_pid, uid, Client_set);
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
				
					Client_set[uid].vec_pipe_table.push_back(pipe_table);		
					
					args_execute(vec_command[i], same_pipe_table_number, last_pid, uid, Client_set);
				}
				
			}  
			else if(vec_command[i].pipe_type == no_pipe_exclamtion || vec_command[i].pipe_type == include_input_text)  //last command, no pipe or exclamation, don't need to create pipe
			{
				args_execute(vec_command[i], same_pipe_table_number,last_pid, uid, Client_set);
			}
		
			for(int j=0;j<Client_set[uid].vec_pipe_table.size();j++)
			{
				if(Client_set[uid].vec_pipe_table[j].counter == 0)
				{	
					close(Client_set[uid].vec_pipe_table[j].fd_in);
					close(Client_set[uid].vec_pipe_table[j].fd_out);
					Client_set[uid].vec_pipe_table.erase(Client_set[uid].vec_pipe_table.begin()+j);
				}
				Client_set[uid].vec_pipe_table[j].counter -= 1;
			}			
			
		}
			
		if(PipeN_number == -1)
		{
			int status;
			waitpid(last_pid, &status, 0);
		}

		vec_command.clear();
		return false;
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
	command_content.user_pipe_type = -1;
	command_content.pipe_from_user = -1;
	command_content.pipe_to_user = -1;
}

void args_execute(Com_content command_content, int same_pipe_table_number, pid_t &last_pid, int uid, vector<client>&Client_set)
{
	//cout << "command: " << command_content.command << endl;
	//cout << "args_execute" << endl;
	signal(SIGCHLD, childHandler);
	pid_t pid;

	bool error_happen = user_pipe_operation(uid, command_content, Client_set);
	if(error_happen == true)
		return;
	
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
					close(Client_set[uid].vec_pipe_table[same_pipe_table_number].fd_out);
					dup2(Client_set[uid].vec_pipe_table[same_pipe_table_number].fd_in,STDOUT_FILENO);	
					close(Client_set[uid].vec_pipe_table[same_pipe_table_number].fd_in);
				}
				else if(command_content.pipe_type == include_exclamation)
				{
					close(Client_set[uid].vec_pipe_table[same_pipe_table_number].fd_out);
					dup2(Client_set[uid].vec_pipe_table[same_pipe_table_number].fd_in,STDOUT_FILENO);	
					dup2(Client_set[uid].vec_pipe_table[same_pipe_table_number].fd_in,STDERR_FILENO);
					close(Client_set[uid].vec_pipe_table[same_pipe_table_number].fd_in);							
				}
			}	
			else  // used new table
			{
				int size = Client_set[uid].vec_pipe_table.size();
				if(command_content.pipe_type == include_pipe)
				{	
					close(Client_set[uid].vec_pipe_table[size-1].fd_out);
					dup2(Client_set[uid].vec_pipe_table[size-1].fd_in,STDOUT_FILENO);	
					close(Client_set[uid].vec_pipe_table[size-1].fd_in);
				}
				else if(command_content.pipe_type == include_exclamation)
				{
					close(Client_set[uid].vec_pipe_table[size-1].fd_out);
					dup2(Client_set[uid].vec_pipe_table[size-1].fd_in,STDOUT_FILENO);
					dup2(Client_set[uid].vec_pipe_table[size-1].fd_in,STDERR_FILENO);
					close(Client_set[uid].vec_pipe_table[size-1].fd_in);							
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

		//for user pipe
		if(command_content.user_pipe_type == only_read_from_user_pipe)
		{			
			int sender_id = command_content.pipe_from_user;
			int receiver_id = uid;
			//connect user pipe
			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_in);
			dup2(Client_set[receiver_id].user_pipe_table[sender_id].fd_out,STDIN_FILENO);
			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_out);
	
		}
		else if(command_content.user_pipe_type == only_write_to_user_pipe)
		{
			int sender_id = uid;
			int receiver_id = command_content.pipe_to_user;
			
			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_out);
			dup2(Client_set[receiver_id].user_pipe_table[sender_id].fd_in,STDOUT_FILENO);
			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_in);

		}
		else if(command_content.user_pipe_type == both_read_and_write_user_pipe)
		{
			//read
			int sender_id = command_content.pipe_from_user;
			int receiver_id = uid;
			
			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_in);
			dup2(Client_set[receiver_id].user_pipe_table[sender_id].fd_out,STDIN_FILENO);			
			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_out);


			//write
			int sender_id_for_write = uid;
			int receiver_id_for_write = command_content.pipe_to_user;
			close(Client_set[receiver_id_for_write].user_pipe_table[sender_id_for_write].fd_out);
			dup2(Client_set[receiver_id_for_write].user_pipe_table[sender_id_for_write].fd_in,STDOUT_FILENO);
			close(Client_set[receiver_id_for_write].user_pipe_table[sender_id_for_write].fd_in);
		}

		//get prior pipetable && check if prior pipetable pipe to me
		for(int j=0;j<Client_set[uid].vec_pipe_table.size();j++)
		{
			if(Client_set[uid].vec_pipe_table[j].counter == 0)  //someone want to give me pipe
			{	
				close(Client_set[uid].vec_pipe_table[j].fd_in);
				dup2(Client_set[uid].vec_pipe_table[j].fd_out,STDIN_FILENO);
				close(Client_set[uid].vec_pipe_table[j].fd_out);
			}
			
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
		//for user pipe
		if(command_content.user_pipe_type == only_read_from_user_pipe)
		{			
			int sender_id = command_content.pipe_from_user;
			int receiver_id = uid;
			string user_pipe_command = command_content.original_command;
			char msg[MAX_MSG_SIZE + 1];
			
			//broadcast msg
			snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just received from %s (#%d) by  '%s'  ***""\n" ,  
			Client_set[receiver_id].name ,receiver_id,  Client_set[sender_id].name, sender_id, user_pipe_command.c_str());
			broadcast(msg, Client_set);

			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_in);
			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_out);

			//after read the user pipe => clear user pipe information
			Client_set[receiver_id].user_pipe_table[sender_id].fd_in = -1;
			Client_set[receiver_id].user_pipe_table[sender_id].fd_out = -1;
			Client_set[receiver_id].user_pipe_table[sender_id].user_pipe_exist = 0;			
		}
		else if(command_content.user_pipe_type == only_write_to_user_pipe)
		{
			string user_pipe_command = command_content.original_command;
			char msg[MAX_MSG_SIZE + 1];
			int sender_id = uid;
			int receiver_id = command_content.pipe_to_user;
			snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n" ,  
			Client_set[sender_id].name ,sender_id, user_pipe_command.c_str(), Client_set[receiver_id].name, receiver_id);
			broadcast(msg, Client_set);	
		}
		else if(command_content.user_pipe_type == both_read_and_write_user_pipe)
		{
			string user_pipe_command = command_content.original_command;
			char msg[MAX_MSG_SIZE + 1];
			//read
			int sender_id = command_content.pipe_from_user;
			int receiver_id = uid;
			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_in);
			close(Client_set[receiver_id].user_pipe_table[sender_id].fd_out);
			//after read the user pipe => clear user pipe information
			Client_set[receiver_id].user_pipe_table[sender_id].fd_in = -1;
			Client_set[receiver_id].user_pipe_table[sender_id].fd_out = -1;
			Client_set[receiver_id].user_pipe_table[sender_id].user_pipe_exist = 0;

			//broadcast msg
			snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just received from %s (#%d) by  '%s'  ***""\n" ,  
			Client_set[receiver_id].name ,receiver_id,  Client_set[sender_id].name, sender_id, user_pipe_command.c_str());
			broadcast(msg, Client_set);

			//write
			int sender_id_for_write = uid;
			int receiver_id_for_write = command_content.pipe_to_user;
			snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n" , 
			Client_set[sender_id_for_write].name ,sender_id_for_write, user_pipe_command.c_str(), Client_set[receiver_id_for_write].name, receiver_id_for_write);
			broadcast(msg, Client_set);
			/*
			//write
			int sender_id_for_write = uid;
			int receiver_id_for_write = command_content.pipe_to_user;
			close(Client_set[receiver_id_for_write].user_pipe_table[sender_id_for_write].fd_in);
			*/
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


bool find_exist_user(int id, vector<client>&Client_set)
{
	//cout << "set size: " << Client_set.size() << endl;
	//cout << Client_set[0].id << endl;
	for(int i=1;i< MAX_CLIENT;i++)
	{
		if(id == Client_set[i].id)
			return true;
	}
	return false;
}

void who(int uid, vector<client>&Client_set)
{
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
}


void tell(int uid, int tell_id, string tell_string, vector<client>&Client_set)
{
	char msg[MAX_MSG_SIZE + 1];
	
	if(Client_set[tell_id].used == false)
	{	
		cout << "*** Error: user #" << tell_id  << " does not exist yet. ***" << endl;
	}
	else
	{
		// user exist
		snprintf (msg, MAX_MSG_SIZE + 1,"*** %s told you ***: %s\n",  Client_set[uid].name, tell_string.c_str());	
		if(send(Client_set[tell_id].my_sockfd, msg, strlen(msg), 0) < 0)
		{
            cerr << "Tell Send Error" << endl; 
        }
	}
}

void  yell(int uid, string yell_string, vector<client>&Client_set)
{
	char msg[MAX_MSG_SIZE + 1];
    snprintf (msg, MAX_MSG_SIZE + 1, "*** %s yelled ***: %s\n",  Client_set[uid].name, yell_string.c_str());
	
	broadcast(msg, Client_set);
}



void name(int uid, string client_new_name, vector<client>&Client_set)
{
	bool name_exist = false;
	char msg[MAX_MSG_SIZE + 1];

    for(int i=1; i<MAX_CLIENT;i++)
    {
		if(Client_set[i].used == true)
		{
			if(strcmp(Client_set[i].name, strdup(client_new_name.c_str())) == 0)
			{
				//if other user used the name
				cout << "*** User '"+ client_new_name + "' already exists. ***" << endl;
				name_exist = true;
				break;
			}	
		}
	}
	if(name_exist == false)
	{
		strncpy(Client_set[uid].name, strdup(client_new_name.c_str()), sizeof(Client_set[uid].name));
        snprintf (msg, MAX_MSG_SIZE + 1, "*** User from %s:%d is named '%s'. ***\n", 
		Client_set[uid].addr, Client_set[uid].port, Client_set[uid].name);
		broadcast(msg, Client_set);
	}
}


void broadcast(char *msg, vector<client>&Client_set)
{
    for(int i=1; i<MAX_CLIENT;i++)
    {
		if(Client_set[i].used == true)
		{
        	if(send(Client_set[i].my_sockfd, msg, strlen(msg), 0) < 0)
        	{
            	cerr << "Broadcast Send Error" << endl; 
        	}
		}
    }
}


bool user_pipe_operation(int uid, Com_content command_content, vector<client>&Client_set)
{
	// for user pipe
	if(command_content.user_pipe_type != -1)
	{
		//cout << "User pipe type: " << command_content.user_pipe_type << endl;
		if(command_content.user_pipe_type == only_read_from_user_pipe)
		{
			bool error_happen = get_msg_from_user_pipe(command_content.pipe_from_user, uid, Client_set); // sender , receiver, msg
			return error_happen;
		}
		else if(command_content.user_pipe_type == only_write_to_user_pipe)
		{
			bool error_happen = write_msg_to_user_pipe(uid, command_content.pipe_to_user, Client_set);
			return error_happen;
		}
		else if(command_content.user_pipe_type == both_read_and_write_user_pipe)
		{
			//READ
			bool  error_happen = get_msg_from_user_pipe(command_content.pipe_from_user, uid, Client_set);    //sender , receiver 
			//cout << "both read write: read :" << endl;
			if(error_happen == true)
				return true;
			else
			{
				//WRITE
				//cout << "sender: " << uid << endl;
				//cout << "receiver" << command_content.pipe_to_user << endl;
				bool error_happen_w = write_msg_to_user_pipe(uid, command_content.pipe_to_user, Client_set);  // sender, receiver
				//cout << "both read write: write :" << endl;
				return error_happen_w;		
			}
		}
	}
	return false;
}

bool write_msg_to_user_pipe(int sender_id, int receiver_id, vector<client>&Client_set)
{
	char msg[MAX_MSG_SIZE + 1];
	bool receiver_exist = true;
	//check whether the user exist
	if(find_exist_user(receiver_id, Client_set) == false)
	{	
		cout << "*** Error: user #" << receiver_id  << " does not exist yet. ***" << endl;
		receiver_exist = false;
	}

	if(receiver_exist == true)
	{
		if(Client_set[receiver_id].user_pipe_table[sender_id].user_pipe_exist == 0)
		{
			// pipe not exist
			int user_pipefd[2];
			if (pipe(user_pipefd) < 0) 
			{
				cerr << "user pipe create fail" << endl;
				exit(1); 
			}

			Client_set[receiver_id].user_pipe_table[sender_id].user_pipe_exist = 1;
			Client_set[receiver_id].user_pipe_table[sender_id].fd_in = user_pipefd[1];
			Client_set[receiver_id].user_pipe_table[sender_id].fd_out = user_pipefd[0];	
			return false;
		}
		else
		{
			//user pipe already exist
			cout << "*** Error: the pipe #" <<  sender_id << "->#" << receiver_id << " already exists. ***" << endl;
			return true; //error happen
		}
	}
	else
		return true; // error happen
}



bool get_msg_from_user_pipe(int sender_id, int receiver_id, vector<client>&Client_set)
{
	char msg[MAX_MSG_SIZE + 1];
	bool sender_exist = true;
	//check whether the user exist
	if(find_exist_user(sender_id, Client_set) == false)
	{	
		cout << "*** Error: user #" << sender_id  << " does not exist yet. ***" << endl;
		sender_exist = false;
	}

	if(sender_exist == true)
	{
		if(Client_set[receiver_id].user_pipe_table[sender_id].user_pipe_exist == 1)
		{
			//cout << "the user pipe exist, can read" << endl;
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

void set_client_env(int uid, vector<client>&Client_set)
{
	//cout << "size : " <<  Client.client_env_map.size() << endl;
	for(const auto&  env:  Client_set[uid].client_env_map)  //only want to read the element in map
	{
		//cout << "key: " << env.first << endl;
		//cout << "value:" << env.second << endl;
		setenv(env.first.c_str(), env.second.c_str(), 1);
	}
}
