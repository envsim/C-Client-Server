#ifndef TCP_SERVER
#define TCP_SERVER

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include "tcpserver.h"

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT "27015"
#define NUMBER_OF_MAX_CLIENTS 10


class TCPServer
{
public:
	TCPServer(char *argv);
	~TCPServer();

	bool isDataFull(std::vector<std::string> &data);
	bool writeDataToFile(std::vector<std::string> &data);
	std::string extract(char *buf, unsigned int size);

	bool start();
	bool stop();
private:
	bool initializeWinsock();
	bool resolveAddressAndPort();
	bool createListenSocket();
	bool setupListening();

	static DWORD WINAPI clientAcceptanceThreadCreate(LPVOID lParam);
	DWORD clientAcceptanceThreadStart();

	static DWORD WINAPI newClientThreadCreate(LPVOID ClientSocket_);
	DWORD newClientThreadStart();
private:
	WSADATA wsaData;
	SOCKET listenSocket;
	SOCKET clientSocket;
	addrinfo *addrInfo;

	CRITICAL_SECTION csOut;

	std::vector<HANDLE> hClients;
	std::vector<SOCKET> hSockets;

	std::vector<HANDLE> eventOfEndThreads;

	std::string directory;

	HANDLE jsonFile;
	HANDLE acceptanceThread;
	HANDLE eventClientAcceptanceThread;
	DWORD dwClientAcceptanceThreadId;

	bool socketLifes;
	bool isListening;
	bool previousEntry;
};
#endif
