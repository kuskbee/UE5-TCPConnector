#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32")


int main()
{
	WSAData wsaData;
	int Result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (Result != 0)
	{
		std::cerr << "WSAStartup failed: " << Result << std::endl;
		return 1;
	}

	SOCKET ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ServerSocket == INVALID_SOCKET)
	{
		std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}

	sockaddr_in ServerSockAddr;
	memset(&ServerSockAddr, 0, sizeof(ServerSockAddr));
	ServerSockAddr.sin_family = AF_INET;
	ServerSockAddr.sin_port = htons(8881);
	inet_pton(AF_INET, "127.0.0.1", &ServerSockAddr.sin_addr.s_addr);
	Result = connect(ServerSocket, (sockaddr*)&ServerSockAddr, sizeof(ServerSockAddr));
	if (Result == SOCKET_ERROR)
	{
		std::cerr << "server connect failed: " << WSAGetLastError() << std::endl;
		closesocket(ServerSocket);
		WSACleanup();
		return 1;
	}

	char buffer[1024] = { 0, };
	int RecvBytes = recv(ServerSocket, buffer, sizeof(buffer), 0);
	if (RecvBytes <= 0)
	{
		std::cerr << "recv failed! recv bytes : "<< RecvBytes<< ", errorcode : " << WSAGetLastError() << std::endl;
		closesocket(ServerSocket);
		WSACleanup();
		return 1;
	}

	std::cout << "Server Send Msg : " << buffer << std::endl;

	closesocket(ServerSocket);
	WSACleanup();
}