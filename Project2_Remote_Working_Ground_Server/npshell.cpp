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
#include <sys/socket.h>
#include <netinet/in.h>

#include "npshell.h"

using namespace std;

const int no_pipe_exclamtion = 0;
const int include_pipe = 1;
const int include_exclamation = 2;
const int include_input_text = 3;


vector<pid_t> pid_table;

void NPShell(int sockfd)
{  
	string Command; 
	//string PATH = "bin:.";
	vector<Com_content> vec_command;
	vector<string> split_command;
	
	vector<Pipe_table> vec_pipe_table;  // vector for storing pipetable

	setenv("PATH", "bin:.", 1);
	
	
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
				
				for(int i=size;i=0;i--)
				{
					//cout << "command: " << int(Command[i] << endl;
					if(Command[i] == '\r' || Command[i] == '\n')
						Command.erase(Command.begin()+i); 
				}
			
				//Command.erase(Command.end()-1,Command.end());
				/*
				for(int i=0;i<Command.size();i++)
				{
					cout << Command[i] << endl;
				}
				*/
				if(Command.size() == 0)
					continue;
				split_command = split_space(Command);

				/* ---------------------------printenv,setenv-------------------------------*/		
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
					exit(0);
				}
				
		/* ---------------------------command---------------------------------------*/			
				while(split_command.size() != 0)
				{
					Com_content command_content;
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
					while(split_command[0][0] != '|' && split_command[0][0] != '!' && split_command[0][0] != '>') //parameter
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
					
					if(split_command.size() != 0)
					{
						if(split_command[0][0] == '|')  // pipe 
						{
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
							split_command.erase(split_command.begin());					
						}
						else if(split_command[0][0] == '>')
						{
							command_content.pipe_to_num = -1;
							command_content.pipe_type = include_input_text;
							split_command.erase(split_command.begin());
							command_content.parameter.push_back(split_command[0]);
							vec_command.push_back(command_content);	
							split_command.erase(split_command.begin());					
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
						else if(vec_command[i].pipe_type == no_pipe_exclamtion || vec_command[i].pipe_type == include_input_text)  //last command, no pipe or exclamation
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

int exist(const char *name)
{
	struct stat buffer;
	return (stat (name, &buffer) == 0);
}


void args_execute(Com_content command_content, vector<Pipe_table> vec_pipe_table, int same_pipe_table_number, pid_t &last_pid)
{
	//cout << "args_execute" << endl;
	signal(SIGCHLD, childHandler);
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


