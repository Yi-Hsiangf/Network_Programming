#include <iostream>
#include <vector>
#include <string>
#include <sstream>
using namespace std;

struct Host_INFO{
    string host_name;
    string host_port;
    string host_file_name;
    int session_id;
};

struct Socks_INFO{
    string socks_name;
    string socks_port;   
};

void Parse_Query_String(Host_INFO *Host, int &count_host, Socks_INFO *Socks);
void Print_HTML_background(Host_INFO *Host, int count_host);
vector<string> split_string_by_delimiter(string str, char delimiter);

//string html_escape(const string& str);
void output_shell(int session_id, string content);
void output_command(int session_id, string content);
bool host_port_file_exist(Host_INFO Host);
bool socks_exist(Socks_INFO Socks);
void html_escape(string& str);
