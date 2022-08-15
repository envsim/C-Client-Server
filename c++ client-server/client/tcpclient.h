#ifndef TCP_CLIENT
#define TCP_CLIENT

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#include <iostream>
#include <string>
#include <vector>
#include <ctime>

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT "27015"

class TCPClient
{
public:
	TCPClient();
	~TCPClient();

	int firstSymb(std::string str); 
	void swapSlashSymb(std::string &str);

	bool connectToServer(char *ip);
	void disconnect();
private:
	bool initializeWinsock();
	bool resolveAddressAndPort();
	bool attemptToConnect();
	void setUp();
	bool startCollAndDisThreads();

	void addWnd(HWND temp);

	void toJson();

	static DWORD WINAPI collThreadCreate(LPVOID lParam);
	DWORD collThreadStart();

	static DWORD WINAPI disThreadCreate(LPVOID lParam);
	DWORD disThreadStart();

	bool getAllVisibleChild(HWND hwnd, bool tie);
private:
	WSADATA wsaData;
	SOCKET connectSocket;

	addrinfo *result;
	addrinfo *ptr;
	addrinfo hints;

	HANDLE collFinished;
	HANDLE collThread;
	HANDLE disThread;
	HANDLE disThreadClose;
	HANDLE collThreadClose;

	HANDLE timerColl;

	std::string name;
	std::string iPAdr;

	unsigned long collPeriod;
	unsigned long disPeriod;

	std::vector<std::string> windows;

	bool serverStatus;
	bool previousColl;

	std::string buferWH;

	int deep;
};
#endif