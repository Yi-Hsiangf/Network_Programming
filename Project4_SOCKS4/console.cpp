#include <array>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <memory>
#include <utility>
#include <unistd.h>
#include "console.hpp"
#include "socks4_client.hpp"

using namespace std;
using namespace boost::asio;

io_service cgi_io_context_;

class CGI_ShellSession : public enable_shared_from_this<CGI_ShellSession> {
    private:
        enum { max_length = 1024 };
        ip::tcp::socket cgi_socket_;
        ip::tcp::resolver resolver_;
        ip::tcp::resolver::query query_;
        ip::tcp::resolver::query socks_query_;
        array<char, max_length> data_;
        ifstream fin;
        Host_INFO host_;
        Socks_INFO socks_;
        socks4_client::client_get_reply cgi_get_reply;

    public:
        CGI_ShellSession(Host_INFO Host, Socks_INFO Socks) 
        : query_(Host.host_name, Host.host_port),
          socks_query_(Socks.socks_name, Socks.socks_port),
          resolver_(cgi_io_context_),
          cgi_socket_(cgi_io_context_) 
          {
            host_ = Host;
            socks_ = Socks;
            fin.open(host_.host_file_name);
          }

        void start() 
        { 
            cout << "In strat" << endl;
            if(socks_exist(socks_) == true)
            {   
                cout << "socks_exist" << endl;
                Resolve_socks_(socks_query_);
            }
            else
            {
                cout << "socks not exist" << endl;
               // Resolve_host_(); 
            }
        }

    private:
        /*
        void Resolve_host_()  // if socks fail, don't use it!
        {
            auto self(shared_from_this());  
            resolver_.async_resolve(query_, [this,self]
            (boost::system::error_code ec, ip::tcp::resolver::iterator iter)
            {
                if(!ec)
                {
                    Connect_host_(iter);
                }
            });
        }
        
        void Connect_host_(ip::tcp::resolver::iterator iter)
        {
            auto self(shared_from_this());
            //cout << "connecting host" << endl;
            cgi_socket_.async_connect(*iter, 
            [this,self](boost::system::error_code ec)
            {
                if(!ec)
                {
                    cout << "socks not exist, don't use socks" << endl;
                    Do_read_();
                }
            });
        }
        */
        // Socks exist, Resolve it!
        
        void Resolve_socks_(ip::tcp::resolver::query query_) 
        {
            auto self(shared_from_this());  
            resolver_.async_resolve(query_, [this,self]
            (boost::system::error_code ec, ip::tcp::resolver::iterator iter)
            {
                if(!ec)
                {
                    cout << "resolve and read to connect" << endl;
                    Connect_socks_(iter);
                }
                else
                {
                    cout << "resolve socks error" << endl;
                }
                
            });
        }

        
        void Connect_socks_(ip::tcp::resolver::iterator iter)
        {
            auto self(shared_from_this());
            //cout << "connecting host" << endl;
            cgi_socket_.async_connect(*iter, 
            [this,self](boost::system::error_code ec)
            {
                if(!ec)
                {
                    cout << "connect to socks" << endl;
                    ip::tcp::resolver resolver(cgi_io_context_);
                    ip::tcp::resolver::query remote_host_query_(ip::tcp::v4(), host_.host_name, host_.host_port);
                    ip::tcp::endpoint remote_endpoint = *resolver.resolve(remote_host_query_);
                    socks4_client::client_send_request cgi_send_request_to_socks(socks4_client::client_send_request::connect, remote_endpoint);
                    cout << "remote addr : " << remote_endpoint.address().to_string() << endl;
                    cout << "remote port : " << remote_endpoint.port() << endl;
                    write(cgi_socket_, cgi_send_request_to_socks.buffers());
		            cout << "Finish writing to socket" << endl;
                    Get_socks_reply();
                }
                else
                {
                    cerr << "connect socks error " << endl;
                }
            });
        }
        
        
        void Get_socks_reply()
        {
            auto self(shared_from_this());
	        cout << "Get socks reply" << endl;
            cgi_socket_.async_read_some(
                cgi_get_reply.buffers(),
                [this, self](boost::system::error_code ec, size_t length) {
                    if (!ec) 
                    {
			            cout << "status : " << cgi_get_reply.status() << endl;
                        if (cgi_get_reply.status() == socks4_client::client_get_reply::request_granted) 
                        {
                            	cout << "Ready to read" << endl;
				                Do_read_();
                        } 
                        else 
                        {
                            cgi_socket_.close();
                        }

                    }
                    else 
                    {
                        cerr << "get socks reply error" << endl;
                        cgi_socket_.close();
                    }
                });
        }

        void Do_read_()
        {
		    cout << "In do read" << endl;
            auto self(shared_from_this());
            cgi_socket_.async_read_some(
                buffer(data_,max_length),
                [this, self](boost::system::error_code ec, size_t length) 
                {
                    if(!ec)
                    {
                        //cout << "length:" << length << endl;
                        string return_shell_msg;
                        return_shell_msg.append(data_.data(), data_.data()+length);
                        //cout << return_shell_msg.size() << flush; 
                        //cout << return_shell_msg << flush;
                        // output "% " on the broswer 
                        output_shell(host_.session_id, return_shell_msg);
                        //usleep(500);
                        size_t want_next_command = return_shell_msg.find('%');
                        if(want_next_command != string::npos)
                        {
                            //cout << "find % " << endl;
                            string command;
                            getline(fin,command);
                            //cout << "The command is: " << command << endl;
                            output_command(host_.session_id, command + "\n");
                            Do_send_command_(command);
                        }
                        else
                        {
                            Do_read_();
                        }
                        
                    }
                    else
                    {
                        //cout << "error code " << endl;
                        fin.close();
                    }
                    
                });
        }
        
        void Do_send_command_(string command)
        {
            auto self(shared_from_this());
            cgi_socket_.async_send(
                buffer(command + "\n", command.size()+1),
                [this,self](boost::system::error_code ec, size_t /* length */) 
                {
                    if (!ec) 
                    {
                        Do_read_();
                    }
                });
        }
};

int main()
{
    Host_INFO Host[5];
    Socks_INFO Socks;
    int count_host = 0;

    //cout << "In Console.cgi" << endl;
    Parse_Query_String(Host, count_host, &Socks);
    Print_HTML_background(Host, count_host);

    cout << "test reference: Socks name: " << Socks.socks_name << endl;
    cout << "test reference: Socks port: " << Socks.socks_port << endl;
    cout << "count_host :" << count_host << endl;
    for(int i=0;i<count_host;i++)
    {
        if(host_port_file_exist(Host[i]) == true)
            make_shared<CGI_ShellSession>(Host[i], Socks)->start();
    }

    cgi_io_context_.run();

    return 0;
}

void Parse_Query_String(Host_INFO *Host, int &count_host, Socks_INFO *Socks)
{
    string query_string = getenv("QUERY_STRING");
    //cout << "query_string : " << query_string << endl;
    // qstring : h0=nplinux1&p0=1234&f0=t1.txt&h1=nplinux6&p1=5678&f1=t2.txt&h2=&p2=&f2=&h3=...&p3=...&f3=...
    vector<string>split_host_port_file = split_string_by_delimiter(query_string, '&');
    for(int i=0 ; i< (split_host_port_file.size()-2)/3  ; i++) //minus two for sh and sp
    {
        vector<string>split_host_name = split_string_by_delimiter(split_host_port_file[i*3], '=');
        vector<string>split_host_port = split_string_by_delimiter(split_host_port_file[i*3+1], '=');
        vector<string>split_host_file = split_string_by_delimiter(split_host_port_file[i*3+2], '=');
        
        if(split_host_name.size() == 2)
        {
            Host[i].host_name = split_host_name[1];
        }
        if(split_host_port.size() == 2)
        {
            Host[i].host_port = split_host_port[1];
        }
        if(split_host_file.size() == 2)
        {
            Host[i].host_file_name = "test_case/" + split_host_file[1];
            Host[i].session_id = i;
        }
        count_host += 1;
    }

    // for socket information
    int split_size = split_host_port_file.size();
    vector<string>split_socks_name = split_string_by_delimiter(split_host_port_file[split_size-2], '=');
    vector<string>split_socks_port = split_string_by_delimiter(split_host_port_file[split_size-1], '=');
    
    Socks->socks_name = split_socks_name[1];
    Socks->socks_port = split_socks_port[1];
}

bool host_port_file_exist(Host_INFO Host)
{
    if(Host.host_name.size() == 0 || Host.host_port.size() == 0 || Host.host_file_name.size() == 0)
        return false;
    else
        return true;
}

bool socks_exist(Socks_INFO Socks)
{
    if(Socks.socks_name.size() == 0 || Socks.socks_port.size() == 0)
        return false;
    else
        return true;
}


void Print_HTML_background(Host_INFO *Host, int count_host)
{
    string response_msg =   "Content-Type: text/html\n\n"
                            "<!DOCTYPE html>\n"
                            "<html lang=\"en\">\n"
                            "  <head>\n"
                            "    <meta charset=\"UTF-8\" />\n"
                            "    <title>NP Project 3 Console</title>\n"
                            "    <link\n"
                            "      rel=\"stylesheet\"\n"
                            "      href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n"
                            "      integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n"
                            "      crossorigin=\"anonymous\"\n"
                            "    />\n"
                            "    <link\n"
                            "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
                            "      rel=\"stylesheet\"\n"
                            "    />\n"
                            "    <link\n"
                            "      rel=\"icon\"\n"
                            "      type=\"image/png\"\n"
                            "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
                            "    />\n"
                            "    <style>\n"
                            "      * {\n"
                            "        font-family: 'Source Code Pro', monospace;\n"
                            "        font-size: 1rem !important;\n"
                            "      }\n"
                            "      body {\n"
                            "        background-color: #212529;\n"
                            "      }\n"
                            "      pre {\n"
                            "        color: #cccccc;\n"
                            "      }\n"
                            "      b {\n"
                            "        color: #ffffff;\n"
                            "      }\n"
                            "    </style>\n"
                            "  </head>\n"
                            "  <body>\n"
                            "    <table class=\"table table-dark table-bordered\">\n"
                            "      <thead>\n"
                            "        <tr>\n";

    for(int i=0;i<count_host;i++)
    {
        if(host_port_file_exist(Host[i]) == true)
            response_msg += "          <th scope=\"col\">" + Host[i].host_name + ":" + Host[i].host_port + "</th>\n";
    }

    response_msg += "        </tr>\n"
                    "      </thead>\n"
                    "      <tbody>\n"
                    "        <tr>\n";

    for(int i=0;i<count_host;i++)
    {
        if(host_port_file_exist(Host[i]) == true)
            response_msg += "          <td><pre id=\"s" + to_string(i) + "\" class=\"mb-0\"></pre></td>\n";
    }

    response_msg += "        </tr>\n"
                    "      </tbody>\n"
                    "    </table>\n"
                    "  </body>\n"
                    "</html>";

    cout << response_msg << endl;
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

/*
string html_escape(const string& str) {

    string escaped;

    for (auto&& ch : str) escaped += ("&#" + to_string(int(ch)) + ";");

    return escaped;

}
*/

void html_escape(string& str) 
{
    string buffer;
    buffer.reserve(str.size());
    for(size_t pos = 0; pos != str.size(); ++pos) {
        switch(str[pos]) {
            case '\n': buffer.append("&NewLine;");   break;
            case '\r': buffer.append("");            break;
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            default:   buffer.append(&str[pos], 1); break;
        }
    }
    str.swap(buffer);
}


void output_shell(int session_id, string content) 
{
    html_escape(content);
    // <script>document.getElementById('s0').innerHTML += '% ';</script>
    string print_html = "<script>document.getElementById('s" + to_string(session_id) + "').innerHTML += \"" + content + "\";</script>";
    cout << print_html << flush;
}

void output_command(int session_id, string content) 
{
    html_escape(content);
    // <script>document.getElementById('s0').innerHTML += '<b>ls&NewLine;</b>';</script>
    string print_html = "<script>document.getElementById('s" + to_string(session_id) + "').innerHTML += \"<b>" + content + "</b>\";</script>";
    cout << print_html << flush;
}
