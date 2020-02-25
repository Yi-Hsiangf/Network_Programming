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

void Parse_Query_String(Host_INFO *Host, int &count_host);
void Print_HTML_background(Host_INFO *Host, int count_host);
vector<string> split_string_by_delimiter(string str, char delimiter);

//string html_escape(const string& str);
void output_shell(int session_id, string content);
void output_command(int session_id, string content);
bool host_port_file_exist(Host_INFO Host);
void html_escape(string& str);