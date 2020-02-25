#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include "server_func.hpp"

using namespace std;
using namespace boost::asio;

io_service io_context_;

class HttpSession : public enable_shared_from_this<HttpSession> {
 private:
  enum { max_length = 1024 };
  ip::tcp::socket socket_;
  array<char, max_length> data_;
 public:
  HttpSession(ip::tcp::socket socket) : socket_(move(socket)) {}

  void start() { do_read(); }

 private:
  void do_read() 
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
                
                header.SERVER_ADDR = socket_.local_endpoint().address().to_string();
                header.SERVER_PORT = to_string(socket_.local_endpoint().port());
                header.REMOTE_ADDR = socket_.remote_endpoint().address().to_string();
                header.REMOTE_PORT = to_string(socket_.remote_endpoint().port());

                //cout << "Ready to Parse" << endl;
                //Parse
                Parse_HTTP_request(HTTP_request, header);
                

                //cout << "server_qstring: " << header.QUERY_STRING << endl;
                //cout << "Finish Parsing" << endl;
                //Check the file
                bool file_exist = false;
                string response_msg = "";
                file_exist = check_request_file_status(header);
                
                //cout << "Finish checking file status" << endl;
                if(file_exist == true)
                {
                    response_msg = HTTP_response_OK(header);
                }
                else
                {
                    response_msg = HTTP_response_Not_found(header);
                }
                do_write(response_msg, file_exist, header);
            }
        });
  }

  void do_write(string response_msg, bool file_exist, HTTP_header header) 
  {
    auto self(shared_from_this());
    socket_.async_send(
        buffer(response_msg.c_str(), response_msg.size()),
        [this, self, header, file_exist](boost::system::error_code ec, size_t  length ) {
        if(!ec)
        {
            if(file_exist == true)
            {
                //cout << "file exist == true" << endl;
                //Set environment
                Set_environment_varibles(header);

                //FORK
                // Inform the io_context that we are about to fork. The io_context
                // cleans up any internal resources, such as threads, that may
                // interfere with forking.
                io_context_.notify_fork(io_service::fork_prepare);
                int pid = fork();
                if(pid == 0)
                {
                    // Inform the io_context that the fork is finished and that this
                    // is the child process. The io_context uses this opportunity to
                    // create any internal file descriptors that must be private to
                    // the new process.
                    io_context_.notify_fork(io_service::fork_child);


                    //dup2
                    int sockfd = socket_.native_handle();
                    dup2(sockfd, STDIN_FILENO);
                    dup2(sockfd, STDOUT_FILENO);
                    dup2(sockfd, STDERR_FILENO);

                    if(execl(header.REQUEST_URI.c_str(), header.REQUEST_URI.c_str(),  NULL) == -1)
                    {
                        cout << "Content-type:text/html\r\n"
                                "\r\n"
                                "<h1>Execution failed</h1>\r\n";
                    }
                }
                else
                {
                    // Inform the io_context that the fork is finished (or failed)
                    // and that this is the parent process. The io_context uses this
                    // opportunity to recreate any internal resources that were
                    // cleaned up during preparation for the fork.
                    io_context_.notify_fork(boost::asio::io_service::fork_parent);

                    // The parent process can now close the newly accepted socket. It
                    // remains open in the child.
                    socket_.close();
                }
            }
        }
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