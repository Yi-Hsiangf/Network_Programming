#include<iostream>
#include<vector>
#include <sstream>
using namespace std;

// Request line :       GET /console.cgi?h0=1234&p0=1234 HTTP/1.1
// Request header :     Host: nplinux8.cs.nctu.edu.tw:7779

struct HTTP_header {
    string REQUEST_METHOD; //GET
    string REQUEST_URI;    // /console.cgi
    string QUERY_STRING;   // h0=1234&p0=1234
    string SERVER_PROTOCOL;// HTTP/1.1
    string HTTP_HOST;      // nplinux8.cs.nctu.edu.tw:7779
    string SERVER_ADDR;
    string SERVER_PORT;
    string REMOTE_ADDR;
    string REMOTE_PORT;
};

void Parse_HTTP_request(string HTTP_request, HTTP_header &header);
bool check_request_file_status(HTTP_header header);
string HTTP_response_OK(HTTP_header header);
string HTTP_response_Not_found(HTTP_header header);
void Set_environment_varibles(HTTP_header header);
vector<string> split_string_by_delimiter(string str, char delimiter);

