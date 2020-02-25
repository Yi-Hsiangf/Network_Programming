#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;
using namespace boost::asio;

/* ------------------Server Function-------------------------*/
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

/* -------------------Console CGI Function -------------------*/
struct Host_INFO{
    string host_name;
    string host_port;
    string host_file_name;
    int session_id;
};
void Parse_Query_String(Host_INFO *Host, int &count_host, HTTP_header header);
string Print_HTML_background(Host_INFO *Host, int count_host);
vector<string> split_string_by_delimiter(string str, char delimiter);

//string html_escape(const string& str);
string output_shell(int session_id, string content);
string output_command(int session_id, string content);
bool host_port_file_exist(Host_INFO Host);
void html_escape(string& str);


/* ------------------Panel CGI Function----------------------*/
string Exec_Panel_CGI();


io_service io_context_;

/*Console CGI Session*/
//io_service cgi_io_context_;


class HttpSession : public enable_shared_from_this<HttpSession> {
 private:
  enum { max_length = 1024 };
  ip::tcp::socket socket_;
  array<char, max_length> data_;
 
 private:
	 class CGI_ShellSession : public enable_shared_from_this<CGI_ShellSession> {
		private:
			enum { max_length = 5000 };
			ip::tcp::socket remote_socket_;
			ip::tcp::resolver resolver_;
			ip::tcp::resolver::query query_;
			array<char, max_length> data_;
			ifstream fin;
			Host_INFO host_;
			shared_ptr<HttpSession>http_ptr_;
		public:
			CGI_ShellSession(Host_INFO Host, shared_ptr<HttpSession>http_ptr) 
			: query_(Host.host_name, Host.host_port),
			  resolver_(io_context_),
			  remote_socket_(io_context_) 
			  {
				host_ = Host;
				http_ptr_ = http_ptr;
				fin.open(Host.host_file_name);
			  }

			void start() 
			{ 
			  cout << "CGI start" << endl;
			  Resolve_host_(); 
			}

		private:
			void Resolve_host_() 
			{
				cout << "resolve_host" <<  endl;
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
				cout << "connecting host" << endl;
				remote_socket_.async_connect(*iter, 
				[this,self](boost::system::error_code ec)
				{
					if(!ec)
					{
						Do_read_Remote_();
					}
                    else
                    {
                        cout << "connect error" << endl;
                    }
                    
				});
			}
			
			void Do_read_Remote_()
			{
				auto self(shared_from_this());
				cout << "READING......." << endl;
				remote_socket_.async_read_some(
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
							string shell_msg = output_shell(host_.session_id, return_shell_msg);
							http_ptr_->Send_HTML_to_brower_(shell_msg);
							//usleep(500);
							size_t want_next_command = return_shell_msg.find('%');
							if(want_next_command != string::npos)
							{
								cout << "find % " << endl;
								string command;
								getline(fin,command);
								//cout << "The command is: " << command << endl;
								string command_msg = output_command(host_.session_id, command + "\n");
								//cout << "command msg: " << command_msg << endl;
								http_ptr_->Send_HTML_to_brower_(command_msg);
								Do_send_command_to_remote_(command);
							}
							else
							{
								Do_read_Remote_();
							}
							
						}
						else
						{
							cout << "error code " << endl;
							fin.close();
						}
						
					});
			}
			
			void Do_send_command_to_remote_(string command)
			{
				auto self(shared_from_this());
				remote_socket_.async_send(
					buffer(command + "\n", command.size()+1),
					[this,self](boost::system::error_code ec, size_t /* length */) 
					{
						if (!ec) 
						{
							Do_read_Remote_();
						}
					});
			}
	};

 public:
  HttpSession(ip::tcp::socket socket) : socket_(move(socket)) {}

  void start() { Do_read_Http_request_(); }

 private:
  void Do_read_Http_request_() 
  {
    auto self(shared_from_this());
    socket_.async_read_some( 
        buffer(data_, max_length), 
        [this, self](boost::system::error_code ec, size_t length) 
        {
            //async_read_some ： 
            //This function is used to asynchronously read data from the stream socket. 
            //The function call always returns immediately.
            
            // (buffer, Readhandler)
            //The handler to be called when the read operation completes.
            if (!ec) 
            {
                HTTP_header header;
                string HTTP_request;
                HTTP_request.append(data_.data(), data_.data()+length);
                cout << "HTTP request: " << HTTP_request << endl;

                header.SERVER_ADDR = socket_.local_endpoint().address().to_string();
                header.SERVER_PORT = to_string(socket_.local_endpoint().port());
                header.REMOTE_ADDR = socket_.remote_endpoint().address().to_string();
                header.REMOTE_PORT = to_string(socket_.remote_endpoint().port());

                cout << "Ready to Parse" << endl;
                //Parse
                Parse_HTTP_request(HTTP_request, header);
                

                cout << "server_qstring: " << header.QUERY_STRING << endl;
                cout << "Finish Parsing" << endl;
                //Check the file
                /*
                bool file_exist = false;
                file_exist = check_request_file_status(header);
                
                cout << "Finish checking file status" << endl;
                */
                bool file_exist = true;
                string response_msg = "";
                response_msg = HTTP_response_OK(header);
                Send_Http_status_code(response_msg, file_exist, header);
            }
        });
  }

  void Send_Http_status_code(string response_msg, bool file_exist, HTTP_header header) 
  {
    auto self(shared_from_this());
    socket_.async_send(
        buffer(response_msg.c_str(), response_msg.size()),
        [this, self, header, file_exist](boost::system::error_code ec, size_t  length ) {
        if(!ec)
        {
            if(file_exist == true)
            {
              cout << "file exist" << endl;
              cout <<  "header.REQUEST_URI: " << header.REQUEST_URI << endl;  
              //Set_environment_varibles(header);
              
              if(header.REQUEST_URI == "panel.cgi")
              {
                string panel_html_string = Exec_Panel_CGI();
				Send_HTML_to_brower_(panel_html_string);
				
              }
              else if(header.REQUEST_URI == "console.cgi")
              {
                cout << "In cosole.cgi " << endl;
                Host_INFO Host[5];
                int count_host = 0;

                //cout << "In Console.cgi" << endl;
                Parse_Query_String(Host, count_host,header);
                string HTML_background = Print_HTML_background(Host, count_host);
				Send_HTML_to_brower_(HTML_background);
                for(int i=0;i<count_host;i++)
                {
                  make_shared<CGI_ShellSession>(Host[i], shared_from_this())->start();
                }

                  try 
                  {
                    cout << "RUN CGI_service" << endl;
                    //cgi_io_context_.run();
                  } 
                  catch (exception& e) 
                  {
                    cerr << "Exception: " << e.what() << endl;
				  }
              }
            }
        }
    });
  }
  
  void Send_HTML_to_brower_(string response_msg) 
  {
    auto self(shared_from_this());
    socket_.async_send(
        buffer(response_msg.c_str(), response_msg.size()),
        [this, self](boost::system::error_code ec, size_t  length ) {
        if(!ec)
        {}
	});	
  }
};




class HttpServer {
 private:
  ip::tcp::acceptor acceptor_;
  ip::tcp::socket socket_;

 public:
  HttpServer(short port)
      : acceptor_(io_context_, ip::tcp::endpoint(ip::tcp::v4(), port)),
        socket_(io_context_) {
    do_accept();
  }

 private:
  void do_accept() {
    acceptor_.async_accept(socket_, [this](boost::system::error_code ec) 
    {
        if (!ec)
        { 
            make_shared<HttpSession>(move(socket_))->start();
            cout << "Do Accept" << endl;
            do_accept();
        }
        else
        {
            cerr << "Accept error: " << ec.message() << endl;
            do_accept();
        }
    });
  }
};

int main(int argc, char *argv[]) 
{
  short port;
  if(argc == 2)
  {
    port = atoi(argv[1]);
  }
  else if (argc > 2) 
  {
    cerr << "Usage:" << argv[0] << " [port]" << endl;
    return -1;
  }

  try 
  {
    cout << "RUN IO_service" << endl;
    HttpServer server(port);
    io_context_.run();
  } 
  catch (exception& e) 
  {
    cerr << "Exception: " << e.what() << endl;
  }

  return 0;
}


/* ------------------------Server Function-------------------------------*/

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

/*
bool check_request_file_status(HTTP_header header)
{
    
    cout << "FILE PATH: " << header.REQUEST_URI << endl;


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
*/
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

/*
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
*/


/* ------------------------console cgi function--------------------------*/
void Parse_Query_String(Host_INFO *Host, int &count_host, HTTP_header header)
{
    string query_string = header.QUERY_STRING;
    //cout << "query_string : " << query_string << endl;
    // qstring : h0=nplinux1&p0=1234&f0=t1.txt&h1=nplinux6&p1=5678&f1=t2.txt&h2=&p2=&f2=&h3=...&p3=...&f3=...
    vector<string>split_host_port_file = split_string_by_delimiter(query_string, '&');
    for(int i=0 ; i< split_host_port_file.size()/3 ; i++)
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
}

bool host_port_file_exist(Host_INFO Host)
{
    if(Host.host_name.size() == 0 || Host.host_port.size() == 0 || Host.host_file_name.size() == 0)
        return false;
    else
        return true;
}


string Print_HTML_background(Host_INFO *Host, int count_host)
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

    return response_msg;
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


string output_shell(int session_id, string content) 
{
    html_escape(content);
    // <script>document.getElementById('s0').innerHTML += '% ';</script>
    string print_html = "<script>document.getElementById('s" + to_string(session_id) + "').innerHTML += \"" + content + "\";</script>";
    return print_html;
}

string output_command(int session_id, string content) 
{
    html_escape(content);
    // <script>document.getElementById('s0').innerHTML += '<b>ls&NewLine;</b>';</script>
    string print_html = "<script>document.getElementById('s" + to_string(session_id) + "').innerHTML += \"<b>" + content + "</b>\";</script>";
    return print_html;
}

/* ---------------------Panel CGI function------------------------------*/
string Exec_Panel_CGI()
{
  string panel_background = "Content-Type: text/html\n\n"
                            "<!DOCTYPE html>\n"
                            "<html lang=\"en\">\n"
                            "  <head>\n"
                            "    <title>NP Project 3 Panel</title>\n"
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
                            "      href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n"
                            "    />\n"
                            "    <style>\n"
                            "      * {\n"
                            "        font-family: 'Source Code Pro', monospace;\n"
                            "      }\n"
                            "    </style>\n"
                            "  </head>\n"
                            "  <body class=\"bg-secondary pt-5\">\n";

        panel_background += "    <form action=\"console.cgi\" method=\"GET\">\n"
                            "      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\n"
                            "        <thead class=\"thead-dark\">\n"
                            "          <tr>\n"
                            "            <th scope=\"col\">#</th>\n"
                            "            <th scope=\"col\">Host</th>\n"
                            "            <th scope=\"col\">Port</th>\n"
                            "            <th scope=\"col\">Input File</th>\n"
                            "          </tr>\n"
                            "        </thead>\n"
                            "        <tbody>\n";

        // 1~10
        //1
        panel_background += "          <tr>\n"
                            "            <th scope=\"row\" class=\"align-middle\">Session 1</th>\n"
                            "            <td>\n"
                            "              <div class=\"input-group\">\n"
                            "                <select name=\"h0\" class=\"custom-select\">\n"
                            "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>\n"
                            "                </select>\n"
                            "                <div class=\"input-group-append\">\n"
                            "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n"
                            "                </div>\n"
                            "              </div>\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <input name=\"p0\" type=\"text\" class=\"form-control\" size=\"5\" />\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <select name=\"f0\" class=\"custom-select\">\n"
                            "                <option></option>\n"
                            "<option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option><option value=\"t6.txt\">t6.txt</option><option value=\"t7.txt\">t7.txt</option><option value=\"t8.txt\">t8.txt</option><option value=\"t9.txt\">t9.txt</option><option value=\"t10.txt\">t10.txt</option>\n"
                            "              </select>\n"
                            "            </td>\n"
                            "          </tr>\n";
        //2
        panel_background += "          <tr>\n"
                            "            <th scope=\"row\" class=\"align-middle\">Session 1</th>\n"
                            "            <td>\n"
                            "              <div class=\"input-group\">\n"
                            "                <select name=\"h1\" class=\"custom-select\">\n"
                            "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>\n"
                            "                </select>\n"
                            "                <div class=\"input-group-append\">\n"
                            "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n"
                            "                </div>\n"
                            "              </div>\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <input name=\"p1\" type=\"text\" class=\"form-control\" size=\"5\" />\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <select name=\"f1\" class=\"custom-select\">\n"
                            "                <option></option>\n"
                            "<option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option><option value=\"t6.txt\">t6.txt</option><option value=\"t7.txt\">t7.txt</option><option value=\"t8.txt\">t8.txt</option><option value=\"t9.txt\">t9.txt</option><option value=\"t10.txt\">t10.txt</option>\n"
                            "              </select>\n"
                            "            </td>\n"
                            "          </tr>\n";

        //3
        panel_background += "          <tr>\n"
                            "            <th scope=\"row\" class=\"align-middle\">Session 1</th>\n"
                            "            <td>\n"
                            "              <div class=\"input-group\">\n"
                            "                <select name=\"h2\" class=\"custom-select\">\n"
                            "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>\n"
                            "                </select>\n"
                            "                <div class=\"input-group-append\">\n"
                            "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n"
                            "                </div>\n"
                            "              </div>\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <input name=\"p2\" type=\"text\" class=\"form-control\" size=\"5\" />\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <select name=\"f2\" class=\"custom-select\">\n"
                            "                <option></option>\n"
                            "<option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option><option value=\"t6.txt\">t6.txt</option><option value=\"t7.txt\">t7.txt</option><option value=\"t8.txt\">t8.txt</option><option value=\"t9.txt\">t9.txt</option><option value=\"t10.txt\">t10.txt</option>\n"
                            "              </select>\n"
                            "            </td>\n"
                            "          </tr>\n";
        //4
        panel_background += "          <tr>\n"
                            "            <th scope=\"row\" class=\"align-middle\">Session 1</th>\n"
                            "            <td>\n"
                            "              <div class=\"input-group\">\n"
                            "                <select name=\"h3\" class=\"custom-select\">\n"
                            "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>\n"
                            "                </select>\n"
                            "                <div class=\"input-group-append\">\n"
                            "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n"
                            "                </div>\n"
                            "              </div>\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <input name=\"p3\" type=\"text\" class=\"form-control\" size=\"5\" />\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <select name=\"f3\" class=\"custom-select\">\n"
                            "                <option></option>\n"
                            "<option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option><option value=\"t6.txt\">t6.txt</option><option value=\"t7.txt\">t7.txt</option><option value=\"t8.txt\">t8.txt</option><option value=\"t9.txt\">t9.txt</option><option value=\"t10.txt\">t10.txt</option>\n"
                            "              </select>\n"
                            "            </td>\n"
                            "          </tr>\n";
        //5
        panel_background += "          <tr>\n"
                            "            <th scope=\"row\" class=\"align-middle\">Session 1</th>\n"
                            "            <td>\n"
                            "              <div class=\"input-group\">\n"
                            "                <select name=\"h4\" class=\"custom-select\">\n"
                            "                  <option></option><option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option><option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option><option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option><option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option><option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option><option value=\"nplinux6.cs.nctu.edu.tw\">nplinux6</option><option value=\"nplinux7.cs.nctu.edu.tw\">nplinux7</option><option value=\"nplinux8.cs.nctu.edu.tw\">nplinux8</option><option value=\"nplinux9.cs.nctu.edu.tw\">nplinux9</option><option value=\"nplinux10.cs.nctu.edu.tw\">nplinux10</option><option value=\"nplinux11.cs.nctu.edu.tw\">nplinux11</option><option value=\"nplinux12.cs.nctu.edu.tw\">nplinux12</option>\n"
                            "                </select>\n"
                            "                <div class=\"input-group-append\">\n"
                            "                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n"
                            "                </div>\n"
                            "              </div>\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <input name=\"p4\" type=\"text\" class=\"form-control\" size=\"5\" />\n"
                            "            </td>\n"
                            "            <td>\n"
                            "              <select name=\"f4\" class=\"custom-select\">\n"
                            "                <option></option>\n"
                            "<option value=\"t1.txt\">t1.txt</option><option value=\"t2.txt\">t2.txt</option><option value=\"t3.txt\">t3.txt</option><option value=\"t4.txt\">t4.txt</option><option value=\"t5.txt\">t5.txt</option><option value=\"t6.txt\">t6.txt</option><option value=\"t7.txt\">t7.txt</option><option value=\"t8.txt\">t8.txt</option><option value=\"t9.txt\">t9.txt</option><option value=\"t10.txt\">t10.txt</option>\n"
                            "              </select>\n"
                            "            </td>\n"
                            "          </tr>\n";
        
        panel_background += "          <tr>\n"
                            "            <td colspan=\"3\"></td>\n"
                            "            <td>\n"
                            "              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n"
                            "            </td>\n"
                            "          </tr>\n"
                            "        </tbody>\n"
                            "      </table>\n"
                            "    </form>\n"
                            "  </body>\n"
                            "</html>\n";

    
  
  
  return panel_background;
}