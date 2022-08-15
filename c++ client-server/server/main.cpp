#include "tcpserver.h"


int __cdecl main(int argc, char **argv)
{
	TCPServer server(argv[1]);
	server.start();
	std::string userMsg;
	while (userMsg != "exit"){
		std::getline(std::cin, userMsg);
	}
	server.stop();
	return 0;
}