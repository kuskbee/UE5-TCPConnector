#include "server.h"

#include <iostream>
#include <ws2tcpip.h>
#include <Windows.h>

Server::~Server()
{
	Close();
}

bool Server::Initialize()
{
	std::cout << "Server Initialize Start" << std::endl;

	std::string ConfigPath = "config.ini";

	bool bSuccess = DbManager.Initialize(ConfigPath);
	if (false == bSuccess)
	{
		std::cerr << "DatabaseManager Initialize failed" << std::endl;
		return false;
	}

	WSAData wsaData;
	int Result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (Result != 0)
	{
		std::cerr << "WSAStartup failed: " << Result << std::endl;
		return false;
	}

	ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET)
	{
		std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return false;
	}

	sockaddr_in ListenSocketAddr;
	memset(&ListenSocketAddr, 0, sizeof(ListenSocketAddr));
	ListenSocketAddr.sin_family = AF_INET;
	ListenSocketAddr.sin_addr.s_addr = INADDR_ANY; //****
	ListenSocketAddr.sin_port = htons(8881);
	Result = bind(ListenSocket, (sockaddr*)&ListenSocketAddr, sizeof(ListenSocketAddr));
	if (Result == SOCKET_ERROR)
	{
		std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	std::cout << "Server Initialize End" << std::endl;

	return true;
}

void Server::Close()
{
	if (ListenSocket != INVALID_SOCKET)
	{
		closesocket(ListenSocket);
		ListenSocket = INVALID_SOCKET;
	}

	WSACleanup();
}

void Server::Run()
{
	if (ListenSocket == INVALID_SOCKET) 
	{
		std::cerr << "Invalid ListenSocket in Run()." << std::endl;
		return; 
	}

	int Result = listen(ListenSocket, SOMAXCONN);
	if (Result == SOCKET_ERROR)
	{
		std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
		Close();
		return;
	}

	std::cout << "Server is listening on port 8881..." << std::endl;

	while (true)
	{
		sockaddr_in ClientSocketAddr;
		memset(&ClientSocketAddr, 0, sizeof(ClientSocketAddr));
		int AddrLen = sizeof(ClientSocketAddr);
		
		std::cout << "Waiting for client to connect..." << std::endl;
		SOCKET ClientSocket = accept(ListenSocket, (sockaddr*)&ClientSocketAddr, &AddrLen);
		if (ClientSocket == INVALID_SOCKET)
		{
			std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
			continue; // Continue to wait for other clients
		}

		char ClientIP[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &ClientSocketAddr, ClientIP, INET_ADDRSTRLEN);
		std::cout << "Client connected from : " << ClientIP << std::endl;

		std::thread(&Server::HandleClient, this, ClientSocket).detach();
	}

}

void Server::HandleClient(SOCKET ClientSocket)
{
	// Initialize
	std::unique_ptr<sql::Connection, ConnDeleter> ClientConn;
	ClientConn.reset(DbManager.GetConnection());

	const char* HelloMsg = "Welcome to Hell";
	int SendBytes = 0;

	if (!ClientConn)
	{
		goto $END;
	}

	Sleep(1000);
	SendBytes = send(ClientSocket, HelloMsg, (int)strlen(HelloMsg), 0);
	if (SendBytes > 0)
	{
		std::cout << "Sent '" << HelloMsg << "' to the client." << std::endl;
	}
	else
	{
		std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
	}

$END:
	closesocket(ClientSocket);
	std::cout << "Client disconnected." << std::endl;
}
