#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string>
#include <fstream>
#include "socks4_server.hpp"


using namespace std;
using namespace boost::asio;

# define max_length 5000
bool Pass_firewall(string operation, string ip_address);
io_service io_context_;

class SOCKS_Session : public enable_shared_from_this<SOCKS_Session> {
 private:
  ip::tcp::socket socket_;
  ip::tcp::resolver resolver_;
  ip::tcp::acceptor bind_acceptor_;
  array<char, max_length> data_from_client_;
  array<char, max_length> data_from_remote_;
  socks4::request socks_request_;
  ip::tcp::socket remote_socket_;
  array<char, max_length> data_;

  enum relay_direction
  {
    read_from_socks_client,
    read_from_remote_server,
    read_from_both_direction
  };

 public:
  SOCKS_Session(ip::tcp::socket socket) : socket_(move(socket)),
                                          resolver_(io_context_),
                                          bind_acceptor_(io_context_),
                                          remote_socket_(io_context_)                                      
  {}

  void start() { Parse_socks_request_(); }

 private:
  void Parse_socks_request_() 
  {
    auto self(shared_from_this());
    socket_.async_read_some( 
        buffer(data_,max_length), 
        [this, self](boost::system::error_code ec, size_t length) 
        {
            
            if (!ec) 
            {    
              string request;
              request.append(data_.data(), data_.data()+length);
              socks_request_.parse_request(request);
              
              if(socks_request_.command() == "connect")
              {
                  ip::tcp::endpoint dst_endpoint = socks_request_.endpoint();
                  if(socks_request_.print_domain_name().size() != 0)
                  {
                    //For SOCKS 4A
                    //cout << "socks 4a" << endl;
                    ip::tcp::resolver::query query_(socks_request_.print_domain_name(), to_string(dst_endpoint.port()));
                    Do_resolve_(query_);
                  }
                  else
                  {
                    //For SOCKS 4
                    ip::tcp::resolver::query query_(dst_endpoint.address().to_string(), to_string(dst_endpoint.port()));
                    Do_resolve_(query_);
                  }
                         
              }
              else if(socks_request_.command() == "bind")
              {
                unsigned short port(0);     // dynamically choose unuse port
                ip::tcp::endpoint bind_endpoint(ip::tcp::v4(), port);
                bind_acceptor_.open(bind_endpoint.protocol());
                bind_acceptor_.set_option(ip::tcp::acceptor::reuse_address(true));
                bind_acceptor_.bind(bind_endpoint);
                bind_acceptor_.listen();

                socks4::reply socks_reply_(socks4::reply::request_granted, bind_acceptor_.local_endpoint());
                //cout << "bind_acceptor address: " << bind_acceptor_.local_endpoint().address() << endl;

                //First reply msg before accept
                write(socket_, socks_reply_.buffers());
                Do_remote_accept_(socks_reply_);
                
              }
              //do_write(response_msg, file_exist, header);
            }
            
        });
  }
  //for bind mode
  void Do_remote_accept_(socks4::reply socks_reply_)
  {
    auto self(shared_from_this());
    bind_acceptor_.async_accept(remote_socket_, [this, self, socks_reply_](boost::system::error_code ec) {
        if (!ec) 
        {
          cout << "<S_IP>: " << socket_.remote_endpoint().address() << endl;
          cout << "<S_PORT>: " << socket_.remote_endpoint().port() << endl;
          cout << "<D_IP>: " << socks_request_.endpoint().address() << endl;
          cout << "<D_PORT>: " << socks_request_.endpoint().port() << endl;
          cout << "<Command>: " << socks_request_.command() << endl;
          cout << "<uid>: " << socks_request_.print_uid() << endl;
          cout << "<domain name>: " << socks_request_.print_domain_name() << endl;
          
          if(Pass_firewall(socks_request_.operation(), socks_request_.endpoint().address().to_string()) == true)
          {
            cout << "<Reply>: "<< "Accept" << endl;
            cout << "----------------------------------" << endl << flush;
            Send_socks_reply_to_client_(socks_reply_);
          }
          else
          {
            //Firewall reject 
            cout << "<Reply>: "<< "Reject" << endl;
            cout << "----------------------------------" << endl;
            socks4::reply socks_reply(socks4::reply::request_failed, socks_request_.endpoint());
            Send_socks_reply_to_client_(socks_reply);
            socket_.close();
          }
        }
        else 
        {
            cerr << "Remote accept error" << endl;
        }
    });
  }
  
  void Do_resolve_(ip::tcp::resolver::query query_)
  {
    auto self(shared_from_this());
    resolver_.async_resolve(query_, [this,self]
    (boost::system::error_code ec, ip::tcp::resolver::iterator iter)
    {
        if(!ec)
        {
            Connect_remote_(iter);
        }
    });
  }

  void Connect_remote_(ip::tcp::resolver::iterator iter)
  {
      auto self(shared_from_this());
      ip::tcp::endpoint iter_endpoint = *iter;
      //cout << iter_endpoint.address() << endl;
      cout << "<S_IP>: " << socket_.remote_endpoint().address() << endl;
      cout << "<S_PORT>: " << socket_.remote_endpoint().port() << endl;
      cout << "<D_IP>: " << socks_request_.endpoint().address() << endl;
      cout << "<D_PORT>: " << socks_request_.endpoint().port() << endl;
      cout << "<Command>: " << socks_request_.command() << endl;
      cout << "<uid>: " << socks_request_.print_uid() << endl;
      cout << "<domain name>: " << socks_request_.print_domain_name() << endl;
      
      if(Pass_firewall(socks_request_.operation(), iter_endpoint.address().to_string()) == true)
      {
        cout << "<Reply>: "<< "Accept" << endl;
        cout << "----------------------------------" << endl << flush;
        
        //connect remote
        remote_socket_.async_connect(*iter, 
        [this,self](boost::system::error_code ec)
        {
            if(!ec)
            {
              //cout << "connect success" << endl;
              socks4::reply socks_reply_(socks4::reply::request_granted, socks_request_.endpoint());
              Send_socks_reply_to_client_(socks_reply_);
            }
            else
            {
              cout << "connect failed" <<  endl;
              socks4::reply socks_reply_(socks4::reply::request_failed, socks_request_.endpoint());
              Send_socks_reply_to_client_(socks_reply_);
            }
            
        });
      }
      else
      {
        //Firewall reject 
        cout << "<Reply>: "<< "Reject" << endl;
        cout << "----------------------------------" << endl;
        socks4::reply socks_reply(socks4::reply::request_failed, socks_request_.endpoint());
        Send_socks_reply_to_client_(socks_reply);
        socket_.close();
      }
  }  
  
  void Send_socks_reply_to_client_(socks4::reply socks_reply_)
  {
    auto self(shared_from_this());
    socket_.async_send(
        socks_reply_.buffers(),
        [this, self, socks_reply_](boost::system::error_code ec, size_t /*length*/) 
        {
          if(!ec)
          {
            //cout << "Send reply success" << endl;
            if(socks_reply_.status() == socks4::reply::request_granted)
            { 
	            //cout << socks_reply_.buffers().data() << endl;
              Relay_traffic_(read_from_both_direction);
              //cout << "request granted" << endl;
            }
            else
            {
              //cout << "request failed" << endl;
              socket_.close();
              remote_socket_.close();
            }
          }
          else
          {
            cerr << "Send reply error" << endl;
          }
        });        
  }

  void Relay_traffic_(int direction)
  {
    auto self(shared_from_this());

    if(direction == read_from_socks_client)
    {
      socket_.async_read_some(
          buffer(data_from_client_),
          [this, self](boost::system::error_code ec, size_t length) 
          {
            if (!ec) 
            {
              /*
              string client_request; // ex: http request   not socks request
              client_request.append(data_from_client_.data(), data_from_client_.data()+length);
              cout << "client_request: " << client_request << endl;
              */
              Send_traffic_to_remote_(length);
            }
            else
            {
              socket_.close();
              remote_socket_.close();
            }
             
          });        
    }
    else if(direction == read_from_remote_server)
    {
      remote_socket_.async_read_some(
          buffer(data_from_remote_),
          [this, self](boost::system::error_code ec, size_t length) 
          {
            if (!ec) 
            {
              /*
              string remote_reply; // ex: http request   not socks request
              remote_reply.append(data_from_remote_.data(), data_from_remote_.data()+length);
              cout << "remote_reply: " << remote_reply << endl;
              */
              Send_traffic_to_client_(length);
            }
            else
            {
              socket_.close();
              remote_socket_.close();
            }
             
          });  
    }
    else if(direction == read_from_both_direction)
    {
      // between SOCKS and client
      socket_.async_read_some(
          buffer(data_from_client_),
          [this, self](boost::system::error_code ec, size_t length) 
          {
            if (!ec) 
            {
              /*
              string client_request; // ex: http request   not socks request
              client_request.append(data_from_client_.data(), data_from_client_.data()+length);
              cout << "client_request: " << client_request << endl;
              */
              Send_traffic_to_remote_(length);
            }
            else
            {
              socket_.close();
              remote_socket_.close();
            }
             
          });    
      // between SOCKS and remote host/server
      remote_socket_.async_read_some(
          buffer(data_from_remote_),
          [this, self](boost::system::error_code ec, size_t length) 
          {
            if (!ec) 
            {
              /*
              string remote_reply; // ex: http request   not socks request
              remote_reply.append(data_from_remote_.data(), data_from_remote_.data()+length);
              cout << "remote_reply: " << remote_reply << endl;
              */
              Send_traffic_to_client_(length);
            }
            else
            {
              socket_.close();
              remote_socket_.close();
            }
             
          });  
    }
  }

  
  void Send_traffic_to_client_(size_t buf_length) 
  {
    auto self(shared_from_this());
    async_write(socket_, buffer(data_from_remote_, buf_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                  // continue listen remote server
                  Relay_traffic_(read_from_remote_server);
                }
                else 
                {
                    // Most probably client closed socket. Let's close both sockets and exit session.
                    socket_.close(); remote_socket_.close();
                }
            });
  }

  void Send_traffic_to_remote_(size_t buf_length)
  {
    auto self(shared_from_this());
    async_write(remote_socket_, buffer(data_from_client_, buf_length),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (!ec)
                {
                  // continue listen client
                  Relay_traffic_(read_from_socks_client);
                }
                else 
                {
                    // Most probably client closed socket. Let's close both sockets and exit session.
                    socket_.close(); remote_socket_.close();
                }
            });
  }
  
};

class SOCKS_Server {
 private:
  ip::tcp::acceptor acceptor_;
  ip::tcp::socket socket_;

 public:
  SOCKS_Server(short port)
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
          // if a SOCKS client connects, use fork() to tackle with it.
          io_context_.notify_fork(io_service::fork_prepare);
          int pid = fork();
          if(pid == 0)
          {
              // Inform the io_context that the fork is finished and that this
              // is the child process. The io_context uses this opportunity to
              // create any internal file descriptors that must be private to
              // the new process.
              io_context_.notify_fork(io_service::fork_child);
              acceptor_.close();
              make_shared<SOCKS_Session>(move(socket_))->start();
              //cout << "Start Sock session" << endl;
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
              do_accept();
          }
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
    SOCKS_Server server(port);
    io_context_.run();
  } 
  catch (exception& e) 
  {
    cerr << "Exception: " << e.what() << endl;
  }

  return 0;
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


bool Pass_firewall(string operation, string ip_address)
{
  ifstream fin;
  string rule;
  fin.open("socks.conf");
  if(!fin) 
  { 
    cout << "open fail" << endl; 
    return false;
  } 
  else
  {
    while(getline(fin,rule))
    {
      vector<string>token = split_string_by_delimiter(rule, ' ');
      string rule_operation = token[1]; 
      string rule_ip = token[2];

      if(operation == rule_operation)
      {
        if(ip_address == rule_ip)
          return true;

        vector<string>split_ip_address = split_string_by_delimiter(ip_address, '.');
        vector<string>split_rule_ip = split_string_by_delimiter(rule_ip, '.');
        for(int i=0;i<4;i++)
        {
          if(split_rule_ip[i] == "*")
          {
            return true;
          }
          else if(split_rule_ip[i] != split_ip_address[i])
            break;
        }
      }
      //compare next rule
    }
    fin.close();
    return false;
  }
}