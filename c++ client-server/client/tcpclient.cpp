#include "tcpclient.h"


TCPClient::TCPClient()
{
	connectSocket = INVALID_SOCKET;

	result = NULL;
	ptr = NULL;

	collFinished = NULL;
	collThread = NULL;
	disThread = NULL;
	collThreadClose = NULL;
	disThreadClose = NULL;
	timerColl = NULL;

	serverStatus = true;
	previousColl = false;

	buferWH = "000";
	deep = 0;
}

TCPClient::~TCPClient() {}

bool TCPClient::connectToServer(char *ip)
{
	if (ip != NULL)
		this->iPAdr = ip;
	else
		this->iPAdr = "";
	if (!initializeWinsock()) return false;
	if (!resolveAddressAndPort()) return false;
	if (!attemptToConnect()) return false;
	setUp();
	if (!startCollAndDisThreads()) return false;
	return true;
}

bool TCPClient::initializeWinsock()
{
	int status = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (status != 0) {
		std::cout << "WSAStartup failed with error: " << status << std::endl;
		return false;
	}
	return true;
}

bool TCPClient::resolveAddressAndPort()
{
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int status = getaddrinfo(iPAdr.c_str(), DEFAULT_PORT, &hints, &result);
	if (status != 0) {
		std::cout << "getaddrinfo failed with error: " << status << std::endl;
		WSACleanup();
		return false;
	}
	return true;
}

bool TCPClient::attemptToConnect()
{
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (connectSocket == INVALID_SOCKET) {
			std::cout << "socket failed with error: " << WSAGetLastError() << std::endl;
			WSACleanup();
			return false;
		}
		int status = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (status == SOCKET_ERROR) {
			closesocket(connectSocket);
			connectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}
	freeaddrinfo(result);

	if (connectSocket == INVALID_SOCKET) {
		std::cout << "Unable to connect to server!\n";
		WSACleanup();
		return false;
	}
	return true;
}

void TCPClient::setUp()
{
	std::cout << "Enter your name: ";
	std::getline(std::cin, name);

	std::cout << "Enter the collection period(min): ";
	std::cin >> collPeriod;
	collPeriod *= 1000 * 60;

	std::cout << "Enter the dispatch period(min): ";
	std::cin >> disPeriod;
	disPeriod *= 1000 * 60;
}

bool TCPClient::startCollAndDisThreads()
{
	collFinished = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == collFinished) {
		std::cout << "CreateEvent(collFinished) failed with error: " << GetLastError() << std::endl;
		return false;
	}
	collThreadClose = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == collThreadClose) {
		std::cout << "CreateEvent(collThreadClose) failed with error: " << GetLastError() << std::endl;
		return false;
	}
	disThreadClose = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == disThreadClose) {
		std::cout << "CreateEvent(disThreadClose) failed with error: " << GetLastError() << std::endl;
		return false;
	}
	collThread = CreateThread(NULL, 0, collThreadCreate, (void*)this, 0, NULL);
	if (NULL == collThread) {
		std::cout << "CreateThread(collThread) failed with some error\n";
		return false;
	}
	disThread = CreateThread(NULL, 0, disThreadCreate, (void*)this, 0, NULL);
	if (NULL == disThread) {
		std::cout << "CreateThread(disThread) failed with some error\n";
		return false;
	}
	return true;
}

bool TCPClient::getAllVisibleChild(HWND hwnd, bool tie)
{
	HWND foundWnd;
	if (!tie) {
		foundWnd = FindWindowEx(hwnd, NULL, NULL, NULL);
	}
	else {
		foundWnd = FindWindowEx(GetAncestor(hwnd, GA_PARENT), hwnd, NULL, NULL);
	}
	if (foundWnd == NULL) {
		foundWnd = FindWindowEx(GetAncestor(hwnd, GA_PARENT), hwnd, NULL, NULL);
		if (foundWnd == NULL) {
			deep--;
			if (deep <= 0) return true;
			getAllVisibleChild(GetParent(hwnd), true);
		}
		else {
			if (IsWindowVisible(foundWnd)) addWnd(foundWnd);
			getAllVisibleChild(foundWnd, false);
		}
	}
	else {
		if (!tie) deep++;
		if (IsWindowVisible(foundWnd)) addWnd(foundWnd);
		getAllVisibleChild(foundWnd, false);
	}
	return true;
}

void TCPClient::addWnd(HWND temp)
{
	std::string tempStr;
	for (int i = 1; i < deep; i++) tempStr += "      ";
	tempStr += "\"Window - name: ";

	int curLength = GetWindowTextLengthA(temp) + 1;
	char* tempText = new char[curLength];
	GetWindowTextA(temp, tempText, curLength);

	if (curLength > 1) {
		std::string strText(curLength - 1, 0);
		strText = tempText;
		tempStr += strText + " class: ";
	}
	else tempStr += " class: ";

	char* classname = new char[DEFAULT_BUFLEN];
	int sLength = GetClassNameA(temp, classname, DEFAULT_BUFLEN);
	if (sLength > 0) {
		std::string strText2(sLength, 0);
		strText2 = classname;
		tempStr += strText2 + "\":";
	}
	else {
		tempStr += "\":";
	}
	windows.push_back(tempStr);
	delete[] classname;
	delete[] tempText;
}

void TCPClient::toJson()
{
	time_t time = std::time(0);
	struct tm now;
	localtime_s(&now, &time);
	std::string date = std::to_string(now.tm_year + 1900);
	date += "." + std::to_string(now.tm_mon + 1);
	date += "." + std::to_string(now.tm_mday);
	date += " " + std::to_string(now.tm_hour);
	date += ":" + std::to_string(now.tm_min);
	date += ":" + std::to_string(now.tm_sec);
	buferWH += "\"" + name + " " + date + "\"";
	buferWH += ":{\n";

	for (unsigned int i = 0; i < windows.size() - 1; i++) {
		if (firstSymb(windows[i + 1]) < firstSymb(windows[i])) {
			int delt = firstSymb(windows[i]) - firstSymb(windows[i + 1]);
			windows[i] += "{}";
			for (int j = 0; j < delt / 6; j++) windows[i] += "}";
			windows[i] += ",\n";
		}
		else if (firstSymb(windows[i + 1]) == firstSymb(windows[i])) windows[i] += "{},\n";
		else windows[i] += "{\n";
		swapSlashSymb(windows[i]);
		buferWH += windows[i];
	}
	swapSlashSymb(windows[windows.size() - 1]);
	buferWH += windows[windows.size() - 1] + "{}\n}";
	for (int i = 0; i < firstSymb(windows[windows.size() - 1]) / 6; i++) {
		buferWH += "}";
	}
}

int TCPClient::firstSymb(std::string str)
{
	for (unsigned int i = 0; i < str.size(); i++) {
		if (str[i] != ' ') return i;
	}
	return 0;
}

void TCPClient::swapSlashSymb(std::string &str)
{
	for (unsigned int i = 0; i < str.size(); i++) {
		if (str[i] == '\\') str[i] = '/';
	}
}

DWORD WINAPI TCPClient::collThreadCreate(LPVOID lParam)
{
	TCPClient* This = (TCPClient*)lParam;
	return This->collThreadStart();
}

DWORD TCPClient::collThreadStart()
{
	timerColl = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == timerColl) {
		std::cout << "CreateEvent(timerColl) failed with error: " << GetLastError() << std::endl;
		serverStatus = false;
	}
	while (serverStatus) {
		WaitForSingleObject(timerColl, collPeriod);
		ResetEvent(collFinished);
		windows.clear();
		if (serverStatus)
		{
			if (previousColl) buferWH += ",";
			getAllVisibleChild(GetDesktopWindow(), false);
			toJson();
			previousColl = true;
		}
		SetEvent(collFinished);
	}
	SetEvent(collThreadClose);
	std::cout << "collThread closed\n";
	ExitThread(0);
}

DWORD WINAPI TCPClient::disThreadCreate(LPVOID lParam)
{
	TCPClient* This = (TCPClient*)lParam;
	return This->disThreadStart();
}

DWORD TCPClient::disThreadStart()
{
	int status = 0;
	do {
		int checkConnection;
		for (unsigned int i = 0; i < disPeriod / 1000; i++) {
			Sleep(1000);
			if (i % 10 == 0) {
				checkConnection = send(connectSocket, "000111", 6, 0);
				if (checkConnection == -1) {
					serverStatus = false;
					SetEvent(collFinished);
					SetEvent(timerColl);
					break;
				}
			}
		}
		WaitForSingleObject(collFinished, INFINITE);
		buferWH += "111";
		int bytesCount = 0;
		if (serverStatus)
		{
			for (unsigned int i = 0; i < (buferWH.size() / DEFAULT_BUFLEN + 1) && serverStatus; i++) {
				std::string abufer;
				for (int j = 0; j < DEFAULT_BUFLEN; j++) {
					abufer += buferWH[j + i * DEFAULT_BUFLEN];
				}
				status = send(connectSocket, abufer.c_str(), (int)strlen(abufer.c_str()) * sizeof(char), 0);
				if (status == SOCKET_ERROR) {
					std::cout << "send failed with error: " << WSAGetLastError() << std::endl;
					break;
				}
				previousColl = false;
				bytesCount += status;
				abufer.clear();
			}
			std::cout << "Completed sending data, size: " << bytesCount << "bytes. \n";
		}
		buferWH = "000";
	} while (status != SOCKET_ERROR && serverStatus);
	if (status == SOCKET_ERROR) serverStatus = false;
	serverStatus = false;
	std::cout << "Connection closed\n";
	status = shutdown(connectSocket, SD_BOTH);
	if (status == SOCKET_ERROR) {
		std::cout << "shutdown failed with error: " << WSAGetLastError() << std::endl;
	}
	closesocket(connectSocket);
	WSACleanup();
	SetEvent(disThreadClose);
	std::cout << "disThread closed\n";
	ExitThread(0);
}

void TCPClient::disconnect()
{
	serverStatus = false;
	disPeriod = 0;

	SetEvent(timerColl);

	WaitForSingleObject(collThreadClose, INFINITE);
	WaitForSingleObject(disThreadClose, INFINITE);
	system("pause");
	CloseHandle(collFinished);
	CloseHandle(collThread);
	CloseHandle(disThread);
	CloseHandle(disThreadClose);
	CloseHandle(collThreadClose);
	CloseHandle(timerColl);

	result = NULL;
	ptr = NULL;
}