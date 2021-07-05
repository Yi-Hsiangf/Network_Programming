# Network_Programming

## Project1 NPShell
Design a shell with special piping mechanisms
See more in the NP_Project1.pdf

## Project2 Remote Working Ground (rwg) Server
Design 3 kinds of serveres
*  Design a Concurrent connection-oriented server. This server allows one client connect to it.
*  Design a server of the chat-like systems, called remote working systems (rwg). In this system, users can communicate
with other users. You need to use the single-process concurrent paradigm to design this server.
* Design the rwg server using the concurrent connection-oriented paradigm with fifo and shared memory.

The features in the servers
1. Pipe between different users. Broadcast message whenever a user pipe is used.
2. Broadcast message of login/logout information.
3. New commands:
  * who: show information of all users
  * tell: send a message to another user
  * yell: send a message to all users
  * name: change your name
4. All commands in project1 (Shell)

See more in the NP_Project2.pdf


## Project3 Remote Batch System 
Design a Remote Batch System called console.cgi and a simplified HTTP server called http server by using the Boost.Asio.
### Console.cgi
1. The console.cgi parse the connection information (e.g. host, port, file) from the environment
variable QUERY STRING, which is set by the HTTP server
2. After parsing, console.cgi will connect to these servers.
### HTTP Server
The http server is in charge of accepting TCP connections and parsing the HTTP requests.
In this project, the URI of HTTP requests will always be in the form of /$fcgi nameg.cgi (e.g.,
/panel.cgi, /printenv.cgi), and we will only test for the HTTP GET method.
Your http server should parse the HTTP headers and execute the cgi programs (which was specified
by the HTTP request) with the CGI procedure (fork, set environment variable, dup, exec).

See more information in NP Project3 Explained.pdf (with graph) and detail in NP_Project3.pdf

## Project4 SOCKS4
Implement a proxy server by using the SOCKS4 protocol in  the application layer of the OSI model.
* Implement SOCKS4 Server Connect Mode
* Implement SOCKS4 Server Bind Mode
* Implement CGI Proxy
* Implement Firewall
