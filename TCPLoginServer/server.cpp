#include <iostream>

//WinSock
#include <WinSock2.h>
#include <ws2tcpip.h>

//MySQL
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/driver.h>
#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/prepared_statement.h>

//Ini
#include "Utils/IniParser.h"
#include <sstream>

#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mysqlcppconn.lib")

int main()
{
	// get database config
	std::string ConfigPath = "config.ini";
	std::string Section = "Database";
	auto DbConfig = util::IniParser::ParseSection(ConfigPath, Section);

	std::string Host = DbConfig["Host"];
	std::string Port = DbConfig["Port"];
	std::string User = DbConfig["User"];
	std::string Pass = DbConfig["Password"];
	std::string Schema = DbConfig["Schema"];

	// sql
	sql::Driver* driver;

	try
	{
		driver = get_driver_instance();

		std::stringstream Url;
		Url << "tcp://" << Host << ":" << Port;
		
		std::unique_ptr<sql::Connection> conn(
			driver->connect(Url.str(), User, Pass)
		);

		conn->setSchema(Schema);
		
		std::cout << "Database connection successful!" << std::endl;

	}
	catch (sql::SQLException& e)
	{
		std::cerr << "Could not connect to database. Error: " << e.what() << std::endl;
		system("pause");
		exit(1); // DB 연결 실패 시 서버 종료
	}


	WSAData wsaData;
	int Result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (Result != 0)
	{
		std::cerr << "WSAStartup failed: " << Result << std::endl;
		return 1;
	}

	SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET)
	{
		std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}

	sockaddr_in ListenSocketAddr;
	memset(&ListenSocketAddr, 0, sizeof(ListenSocketAddr));
	ListenSocketAddr.sin_family = AF_INET;	
	ListenSocketAddr.sin_addr.s_addr = INADDR_ANY; //****
	ListenSocketAddr.sin_port = htons(8881);
	Result = bind(ListenSocket, (sockaddr*)&ListenSocketAddr, sizeof(ListenSocketAddr));
	if (Result == SOCKET_ERROR) {
		std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	Result = listen(ListenSocket, SOMAXCONN);
	if (Result == SOCKET_ERROR) {
		std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	std::cout << "Server is listening on port 8881..." << std::endl;

	SOCKET ClientSocket;
	sockaddr_in ClientSocketAddr;
	memset(&ClientSocketAddr, 0, sizeof(ClientSocketAddr));
	int ClientAddrSize = sizeof(ClientSocketAddr);

	while (true)
	{
		std::cout << "Waiting for client to connect..." << std::endl;
		ClientSocket = accept(ListenSocket, (sockaddr*)&ClientSocketAddr, &ClientAddrSize);
		if (ClientSocket == INVALID_SOCKET)
		{
			std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
			continue; // Continue to wait for other clients
		}

		char ClientIP[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &ClientSocketAddr, ClientIP, INET_ADDRSTRLEN);
		std::cout << "Client connected from : " << ClientIP << std::endl;

		//
		const char* HelloMsg = "Welcome to Hell";
		int SendResult = send(ClientSocket, HelloMsg, (int)strlen(HelloMsg), 0);
		if (SendResult != SOCKET_ERROR)
		{
			std::cout << "Sent '" << HelloMsg << "' to the client." << std::endl;
		}
		else
		{
			std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
		}

		closesocket(ClientSocket);
		std::cout << "Client disconnected." << std::endl;
	}

	closesocket(ListenSocket);
	WSACleanup();
}