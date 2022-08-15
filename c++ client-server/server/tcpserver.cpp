#include "tcpserver.h"


bool TCPServer::initializeWinsock()
{
	int status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0) {
		std::cout << "WSAStartup failed with error: " << status << std::endl;
		return false;
	}
	return true;
}

bool TCPServer::resolveAddressAndPort()
{
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	int status = getaddrinfo(NULL, DEFAULT_PORT, &hints, &addrInfo);
	if (status != 0) {
		std::cout << "getaddrinfo failed with error: " << status << std::endl;
		WSACleanup();
		return false;
	}
	return true;
}

bool TCPServer::createListenSocket()
{
	listenSocket = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);
	if (listenSocket == INVALID_SOCKET) {
		std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
		freeaddrinfo(addrInfo);
		WSACleanup();
		return false;
	}
	return true;
}

bool TCPServer::setupListening()
{
	int status = bind(listenSocket, addrInfo->ai_addr, (int)addrInfo->ai_addrlen);
	if (status == SOCKET_ERROR) {
		std::cout << "bind failed with error: " << WSAGetLastError() << std::endl;
		freeaddrinfo(addrInfo);
		closesocket(listenSocket);
		WSACleanup();
		return false;
	}

	freeaddrinfo(addrInfo);

	status = listen(listenSocket, SOMAXCONN);
	if (status == SOCKET_ERROR) {
		std::cout << "listen failed with error: " << WSAGetLastError() << std::endl;
		closesocket(listenSocket);
		WSACleanup();
		return false;
	}
	return true;
}

DWORD WINAPI TCPServer::newClientThreadCreate(LPVOID lParam)
{
	TCPServer* This = (TCPServer*)lParam;
	return This->newClientThreadStart();
}

DWORD TCPServer::newClientThreadStart()
{
	std::cout << "A new client is connected!\n";
	SOCKET clientSocket = hSockets[hSockets.size() - 1];
	int currentStatus = 1;
	char recvBuf[DEFAULT_BUFLEN];
	std::vector<std::string> data;
	HANDLE hCurrentClient = hClients[hClients.size() - 1];
	HANDLE hCurrentEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == hCurrentEvent) {
		std::cout << "CreateEvent(hCurrentEvent) failed with error: " << GetLastError() << std::endl;
	}
	else {
		eventOfEndThreads.push_back(hCurrentEvent);
		fd_set readSetOfClientSocket;
		do {
			timeval tmvl = { 1, 0 };
			FD_ZERO(&readSetOfClientSocket);
			FD_SET(clientSocket, &readSetOfClientSocket);
			if (select(NULL, &readSetOfClientSocket, NULL, NULL, &tmvl) == SOCKET_ERROR) {
				std::cout << "select returned with error: " << WSAGetLastError() << std::endl;
				break;
			}
			if (FD_ISSET(clientSocket, &readSetOfClientSocket)) {
				currentStatus = recv(clientSocket, recvBuf, DEFAULT_BUFLEN, 0);
				if (currentStatus == 0) {
					std::cout << "Client disconnected!\n";
				}
				else if (currentStatus == -1) {
					std::cout << "Client lost connected!\n";
				}
				else {
					if (currentStatus > 6)
					{
						data.push_back(extract(recvBuf, currentStatus));
						EnterCriticalSection(&csOut);
						if (isDataFull(data)) {
							writeDataToFile(data);
						}
						LeaveCriticalSection(&csOut);
					}
				}
			}
		} while (currentStatus > 0 && socketLifes);
		hClients.erase(std::remove(hClients.begin(), hClients.end(), hCurrentClient));
	}
	hSockets.erase(std::remove(hSockets.begin(), hSockets.end(), clientSocket));

	currentStatus = shutdown(clientSocket, SD_BOTH);
	if (currentStatus == SOCKET_ERROR) {
		std::cout << "shutdown failed with error: " << WSAGetLastError() << std::endl;
	}
	closesocket(clientSocket);

	if (socketLifes) eventOfEndThreads.erase(std::remove(eventOfEndThreads.begin(), eventOfEndThreads.end(), hCurrentEvent));
	else SetEvent(hCurrentEvent);

	hCurrentEvent = NULL;
	hCurrentClient = NULL;

	std::cout << "Thread of one of the clients successfully finished!\n";
	ExitThread(0);
}

DWORD WINAPI TCPServer::clientAcceptanceThreadCreate(LPVOID lParam)
{
	TCPServer* This = (TCPServer*)lParam;
	return This->clientAcceptanceThreadStart();
}

DWORD TCPServer::clientAcceptanceThreadStart()
{
	fd_set readSetOfListeningSocket;
	while (isListening)
	{
		timeval timeout = { 1, 0 };
		FD_ZERO(&readSetOfListeningSocket);
		FD_SET(listenSocket, &readSetOfListeningSocket);
		if (select(NULL, &readSetOfListeningSocket, NULL, NULL, &timeout) == SOCKET_ERROR) {
			std::cout << "select returned with error: " << WSAGetLastError() << std::endl;
			break;
		}
		if (FD_ISSET(listenSocket, &readSetOfListeningSocket)) {
			clientSocket = accept(listenSocket, NULL, NULL);
			if (clientSocket == INVALID_SOCKET) {
				std::cout << "acceptance failed with error: " << WSAGetLastError() << std::endl;
				break;
			}
			else {
				hSockets.push_back(clientSocket);
				HANDLE hClient = CreateThread(NULL, 0, newClientThreadCreate, this, 0, &dwClientAcceptanceThreadId);
				if (NULL == hClient) {
					std::cout << "CreateThread(hClient) failed with some error\n";
					break;
				}
				hClients.push_back(hClient);
			}
		}
	}
	socketLifes = false;
	closesocket(listenSocket);
	WSACleanup();
	SetEvent(eventClientAcceptanceThread);
	std::cout << "Thread of client accceptance successfully finished!\n";
	ExitThread(0);
}

std::string TCPServer::extract(char *buf, unsigned int size)
{
	std::string str;
	for (unsigned int i = 0; i < size; i++) {
		str += buf[i];
	}
	return str;
}

bool TCPServer::isDataFull(std::vector<std::string> &data)
{
	int indexOfLastObj = data.size() - 1;
	if (data[0][0] == '0' && data[0][1] == '0' && data[0][2] == '0'
		&& data[indexOfLastObj][data[indexOfLastObj].size() - 1] == '1' && data[indexOfLastObj][data[indexOfLastObj].size() - 2] == '1' && data[indexOfLastObj][data[indexOfLastObj].size() - 3] == '1'
		) return true;
	else return false;
}

bool TCPServer::writeDataToFile(std::vector<std::string> &data)
{
	std::string firstStr;
	if (previousEntry) firstStr += ",";
	for (unsigned int i = 3; i < data[0].size(); i++) firstStr += data[0][i];
	data[0] = firstStr;

	std::string lastStr;
	for (unsigned int i = 0; i < (data[data.size() - 1].size() - 3); i++) lastStr += data[data.size() - 1][i];
	lastStr += "}";
	data[data.size() - 1] = lastStr;

	DWORD dwByteCount;

	if (previousEntry) SetFilePointer(jsonFile, -1, NULL, FILE_END);
	else WriteFile(jsonFile, "{", sizeof(char), &dwByteCount, NULL);

	for (unsigned int i = 0; i < data.size(); i++) {
		WriteFile(jsonFile, data[i].c_str(), sizeof(char)*data[i].size(), &dwByteCount, NULL);
	}
	previousEntry = true;
	data.clear();
	std::cout << "Data from one of the users is fully accepted and recorded.\n";
	return true;
}

TCPServer::TCPServer(char *argv)
{
	listenSocket = INVALID_SOCKET;
	clientSocket = INVALID_SOCKET;

	addrInfo = NULL;
	jsonFile = NULL;

	csOut = { 0 };
	directory = argv;
	
	acceptanceThread = NULL;
	eventClientAcceptanceThread = NULL;

	socketLifes = true;
	isListening = true;
	previousEntry = false;
}

bool TCPServer::start()
{
	jsonFile = CreateFileA(directory.c_str(), GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (jsonFile == INVALID_HANDLE_VALUE) {
		std::cout << "CreateFile(jsonFile) failed with error: " << GetLastError() << std::endl;
		return false;
	}
	if (!initializeWinsock()) return false;
	if (!resolveAddressAndPort()) return false;
	if (!createListenSocket()) return false;
	if (!setupListening()) return false;
	std::cout << "Server started!\n";
	std::cout << "Directory: " << directory << ".\n";
	InitializeCriticalSection(&csOut);
	eventClientAcceptanceThread = CreateEvent(NULL, FALSE, FALSE, "endOfClientAcceptance");
	if (NULL == eventClientAcceptanceThread) {
		std::cout << "CreateEvent(eventClientAcceptanceThread) failed with error: " << GetLastError() << std::endl;
		return false;
	}
	acceptanceThread = CreateThread(NULL, 0, clientAcceptanceThreadCreate, this, 0, NULL);
	if (NULL == acceptanceThread) {
		std::cout << "CreateThread(acceptanceThread) failed with some error\n";
		return false;
	}
	return true;
}

bool TCPServer::stop()
{
	HANDLE clientThreads[NUMBER_OF_MAX_CLIENTS];
	socketLifes = false;
	for (unsigned int i = 0; i < eventOfEndThreads.size(); i++) {
		clientThreads[i] = eventOfEndThreads[i];
	}
	WaitForMultipleObjects(eventOfEndThreads.size(), clientThreads, TRUE, INFINITE);
	isListening = false;
	WaitForSingleObject(eventClientAcceptanceThread, INFINITE);
	DeleteCriticalSection(&csOut);
	for (unsigned int i = 0; i < hClients.size(); i++) {
		CloseHandle(hClients[i]);
	}

	for (unsigned int i = 0; i < eventOfEndThreads.size(); i++) {
		CloseHandle(eventOfEndThreads[i]);
	}
	CloseHandle(acceptanceThread);
	CloseHandle(eventClientAcceptanceThread);
	CloseHandle(jsonFile);
	return true;
}

TCPServer::~TCPServer() {}

