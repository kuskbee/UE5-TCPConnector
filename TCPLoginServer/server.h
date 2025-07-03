#pragma once

//DB
#include "DB/DatabaseManager.h"

//WinSock
#include <WinSock2.h>

#include <thread>

class Server
{
public:
	virtual ~Server();

	bool Initialize();
	void Close();
	void Run();
	
private:
	void HandleClient(SOCKET ClientSocket);

	SOCKET ListenSocket{ INVALID_SOCKET };
	DatabaseManager DbManager;
};