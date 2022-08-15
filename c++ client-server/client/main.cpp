#include "tcpclient.h"



int __cdecl main(int argc, char **argv)
{
	TCPClient a;
	a.connectToServer(argv[1]);
	std::string exit;
	while (exit != "exit")
	{
		std::getline(std::cin, exit);
	}
	a.disconnect();
	return 0;
}