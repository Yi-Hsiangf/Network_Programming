all:
	g++ simple_main.cpp npshell.cpp -o np_simple
	g++ -g np2_main.cpp np2_shell.cpp np2_msg.cpp -o np_single_proc
	g++ -g np3_main.cpp np3_shell.cpp np3_msg.cpp -o np_multi_proc
	
clean:
	rm -f np_simple
	rm -f np_single_proc
	rm -f np_multi_proc
