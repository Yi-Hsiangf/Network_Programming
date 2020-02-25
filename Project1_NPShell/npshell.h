#include<iostream>
#include<vector>
#include<sys/stat.h>   
#include<unistd.h>
#include<stdio.h>
#include<string>
#include <string.h>

using namespace std;

class Pipe_table{
	public:
		int fd_in;
		int fd_out;
		int counter;
};


struct Com_content
{
	string command;
	vector<string> parameter;
	int pipe_to_num;
	int pipe_type;
};

vector<string>split_path(string path);
vector<string>split_space(string command);
int exist(const char *name);
void args_execute(Com_content command_content, vector<Pipe_table> vec_pipe_table, vector<string>vec_path, int same_pipe_table_number, pid_t &last_pid);
void childHandler(int signo);
