all:
	g++ http_server.cpp server_func.cpp -o http_server -lboost_system -pthread
	g++ console.cpp -o console.cgi -lboost_system -pthread
clean:
	rm -f http_server
	rm -f console.cgi
