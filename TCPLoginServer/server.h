#pragma once

//DB
#include "DB/DatabaseManager.h"

//WinSock
#include <WinSock2.h>

//Packet
#include "flatbuffers/flatbuffers.h"
#include "LoginProtocol_generated.h"

#include <thread>
#include <vector>

class Server
{
public:
	virtual ~Server();

	bool Initialize();
	void Close();
	void Run();
	
private:
	void HandleClient(SOCKET ClientSocket);

	void ProcessPacket(std::vector<char>& RecvBuf, std::unique_ptr<sql::Connection, ConnDeleter>& ClientConn, SOCKET ClientSocket);

	void ProcessSignUpRequest(const LoginProtocol::MessageEnvelope* MsgEnvelope, std::unique_ptr<sql::Connection, ConnDeleter>& ClientConn, SOCKET ClientSocket);

	void ProcessLoginRequest(const LoginProtocol::MessageEnvelope* MsgEnvelope, std::unique_ptr<sql::Connection, ConnDeleter>& ClientConn, SOCKET ClientSocket);

	bool RecvAll(SOCKET sock, char* buf, size_t len);

	bool SendAll(SOCKET sock, char* buf, size_t len);

	bool SendFlatBufferMessage(SOCKET Sock, flatbuffers::FlatBufferBuilder& Builder);
	bool ReceiveFlatBufferMessage(SOCKET Sock, std::vector<char>& RecvBuf, uint32_t& outMessageSize);

	SOCKET ListenSocket{ INVALID_SOCKET };
	DatabaseManager DbManager;
};