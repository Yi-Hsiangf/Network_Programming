#include "server_func.hpp"
#include<iostream>
#include "unistd.h"



void Parse_HTTP_request(string HTTP_request, HTTP_header &header)
{
    vector<string>split_HTTP_Request_by_line = split_string_by_delimiter(HTTP_request, '\n'); 

    //GET /index.html HTTP/1.1
    string request_line = split_HTTP_Request_by_line[0]; // First line
    vector<string>split_request_line = split_string_by_delimiter(request_line, ' ');
    header.REQUEST_METHOD = split_request_line[0]; //GET
    header.SERVER_PROTOCOL = split_request_line[2]; //HTTP/1.1

    vector<string>split_URI_Query_String = split_string_by_delimiter(split_request_line[1], '?');

    vector<string> file_path = split_string_by_delimiter(split_URI_Query_String[0], '/');

    header.REQUEST_URI = file_path[1];
    if(split_URI_Query_String.size() > 1)
    {
        header.QUERY_STRING = split_URI_Query_String[1];
    }

    // Request header :     Host: nplinux8.cs.nctu.edu.tw:7779
    string request_header = split_HTTP_Request_by_line[1]; // Second line
    vector<string>split_Host_line = split_string_by_delimiter(request_header, ' ');
    vector<string>split_Host_Port = split_string_by_delimiter(split_Host_line[1], ':');
    header.HTTP_HOST = split_Host_Port[0];

}


bool check_request_file_status(HTTP_header header)
{
    
    cout << "FILE PATH: " << header.REQUEST_URI << endl;
    /*
    if(access("/panel.cgi", F_OK) != -1)
    {
        cout << "panel.cgi exist" << endl;
    }
    */

    if(access(header.REQUEST_URI.c_str(), F_OK) != -1) //file exist
    {
        HTTP_response_OK(header);
        return true;
    } 
    else //file not exist
    {
        HTTP_response_Not_found(header);
        return false;
    }
    
}

string HTTP_response_OK(HTTP_header header)
{
    string response_msg;
    response_msg = header.SERVER_PROTOCOL + " 200 OK\r\n";
    return response_msg;
}

string HTTP_response_Not_found(HTTP_header header)
{
    string response_msg;
    response_msg = header.SERVER_PROTOCOL + " 404 Not Found\r\n"
                                            "Content-Length: 20\r\n"
                                            "\r\n"
                                            "<h1>Not Found</h1>\r\n";
    return response_msg;
}

void Set_environment_varibles(HTTP_header header)
{
    setenv("REQUEST_METHOD", header.REQUEST_METHOD.c_str(), 1);
    setenv("REQUEST_URI", header.REQUEST_URI.c_str(), 1);
    setenv("QUERY_STRING", header.QUERY_STRING.c_str(), 1);
    setenv("SERVER_PROTOCOL", header.SERVER_PROTOCOL.c_str(), 1);
    setenv("HTTP_HOST", header.HTTP_HOST.c_str(), 1);
    setenv("SERVER_ADDR", header.SERVER_ADDR.c_str(), 1);
    setenv("SERVER_PORT", header.SERVER_PORT.c_str(), 1);
    setenv("REMOTE_ADDR", header.REMOTE_ADDR.c_str(), 1);
    setenv("REMOTE_PORT", header.REMOTE_PORT.c_str(), 1);
}



vector<string> split_string_by_delimiter(string str, char delimiter)
{
    string token;
    vector<string>splited_string;
    istringstream isstream(str);
    while(getline(isstream, token, delimiter))
    {
        splited_string.push_back(token);
    }
    return splited_string;
}
