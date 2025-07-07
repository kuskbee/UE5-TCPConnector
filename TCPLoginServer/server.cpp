#include "server.h"

#include <iostream>
#include <chrono>
#include <cstdint>
#include <ws2tcpip.h>
#include "Utils/UuidHelper.h"

uint64_t GetTimeStamp()
{
	// 시스템 시계를 UTC epoch(1970-01-01) 기준으로 읽어서
	// 밀리초 단위 정수로 변환
	using namespace std::chrono;
	auto now = system_clock::now();
	auto ms = duration_cast<milliseconds>(now.time_since_epoch());
	return static_cast<uint64_t>(ms.count());
}

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
	sql::Connection* ClientConn = DbManager.GetConnection();

	if (!ClientConn)
	{
		closesocket(ClientSocket);
		std::cerr << "Failed to Get DB Conn, Client disconnected." << std::endl;
		return;
	}

	constexpr size_t READ_CHUNK = 64 * 1024; // 64KB
	std::vector<char> RecvBuf;
	RecvBuf.reserve(READ_CHUNK);

	while(true)
	{
		uint32_t MessageSize = 0;
		bool bRecv = ReceiveFlatBufferMessage(ClientSocket, RecvBuf, MessageSize);
		if (false == bRecv)
		{
			std::cerr << "Failed Receive FlatBuffer Message" << std::endl;
			break;
		}
		
		// FlatBuffer 검증
		flatbuffers::Verifier verifier(
			reinterpret_cast<const uint8_t*>(RecvBuf.data()), MessageSize);

		if (!LoginProtocol::VerifyMessageEnvelopeBuffer(verifier))
		{
			std::cerr << "Invalid FlatBuffer received\n";
			break;
		}

		// 메시지 디스패치
		ProcessPacket(RecvBuf, ClientConn, ClientSocket);
	}
	
	CleanUpClientSession(ClientSocket);

	closesocket(ClientSocket);
	std::cout << "Client disconnected." << std::endl;
}

void Server::ProcessPacket(std::vector<char>& RecvBuf, sql::Connection* ClientConn, SOCKET ClientSocket)
{
	const LoginProtocol::MessageEnvelope* MsgEnvelope = LoginProtocol::GetMessageEnvelope(RecvBuf.data());
	switch (MsgEnvelope->body_type())
	{
	case LoginProtocol::Payload::C2S_LoginRequest:
	{
		ProcessLoginRequest(MsgEnvelope, ClientConn, ClientSocket);
		break;
	}
	case LoginProtocol::Payload::C2S_SignUpRequest:
	{
		ProcessSignUpRequest(MsgEnvelope, ClientConn, ClientSocket);
		break;
	}
	case LoginProtocol::Payload::C2S_PlayerListRequest:
	{
		ProcessPlayerListRequest(MsgEnvelope, ClientConn, ClientSocket);
		break;
	}
	case LoginProtocol::Payload::C2S_GameReadyRequest:
	{
		ProcessGameReadyRequest(MsgEnvelope, ClientConn, ClientSocket);
		break;
	}
	default:
		std::cerr << "Unknown payload type\n";
		break;
	}
}

void Server::ProcessSignUpRequest(const LoginProtocol::MessageEnvelope* MsgEnvelope, sql::Connection* ClientConn, SOCKET ClientSocket)
{
	const LoginProtocol::C2S_SignUpRequest* SignUpReq = MsgEnvelope->body_as_C2S_SignUpRequest();
	std::string UserId = SignUpReq->userid()->str();
	std::string Password = SignUpReq->password()->str();
	std::string Nickname = SignUpReq->nickname()->str();

	SignUpStatus ResultStatus = DbManager.SignUp(
		ClientConn, UserId, Password, Nickname);

	// 응답 FlatBuffer 작성
	flatbuffers::FlatBufferBuilder Builder;
	auto BodyOffset = LoginProtocol::CreateS2C_SignUpResponse(
		Builder,
		static_cast<LoginProtocol::ErrorCode>(ResultStatus)
	);
	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::S2C_SignUpResponse,
		BodyOffset.Union()
	);
	Builder.Finish(SendMsgData);
	SendFlatBufferMessage(ClientSocket, Builder);
}

void Server::ProcessLoginRequest(const LoginProtocol::MessageEnvelope* MsgEnvelope, sql::Connection* ClientConn, SOCKET ClientSocket)
{
	const LoginProtocol::C2S_LoginRequest* LoginReq = MsgEnvelope->body_as_C2S_LoginRequest();
	std::string UserId = LoginReq->userid()->str();
	std::string Password = LoginReq->password()->str();

	int PlayerId;
	std::string Nickname;
	LoginStatus ResultStatus = DbManager.Login(
		ClientConn, UserId, Password, PlayerId, Nickname);

	std::string SessionToken = "-";
	if (ResultStatus == LoginStatus::Success)
	{
		std::unique_lock<std::shared_mutex> Lock(SessionMutex);

		SessionToken = util::UuidHelper::GenerateSessionToken();
		PlayerSession JoinPlayer = PlayerSession{
			PlayerId, UserId, Nickname, ClientSocket,
			PlayerState::Lobby
		};

		BroadcastPlayerJoin(JoinPlayer);

		PlayerSessionMap[SessionToken] = JoinPlayer;
		TokenByPlayerIdMap[PlayerId] = SessionToken;
		TokenBySocketMap[ClientSocket] = SessionToken;
	}

	// 응답 FlatBuffer 작성
	flatbuffers::FlatBufferBuilder Builder;
	auto NickOffset = Builder.CreateString(Nickname);
	auto TokenOffset = Builder.CreateString(SessionToken);
	auto BodyOffset = LoginProtocol::CreateS2C_LoginResponse(
		Builder,
		static_cast<LoginProtocol::ErrorCode>(ResultStatus),
		TokenOffset,
		NickOffset
	);
	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::S2C_LoginResponse,
		BodyOffset.Union()
	);
	Builder.Finish(SendMsgData);
	SendFlatBufferMessage(ClientSocket, Builder);
}

void Server::ProcessPlayerListRequest(const LoginProtocol::MessageEnvelope* MsgEnvelope, sql::Connection* ClientConn, SOCKET ClientSocket)
{
	flatbuffers::FlatBufferBuilder Builder;

	std::vector<flatbuffers::Offset<LoginProtocol::Player>> Vec;
	for (const auto& Player : PlayerSessionMap)
	{
		auto UserId = Builder.CreateString(Player.second.UserId);
		auto Nickname = Builder.CreateString(Player.second.PlayerName);
		Vec.emplace_back(LoginProtocol::CreatePlayer(
			Builder,
			UserId,
			Nickname,
			static_cast<LoginProtocol::PlayerState>(Player.second.CurrentState)
		));
		std::cout << "[PlayerList] UserId : " << Player.second.UserId;
		std::cout << ", Nickname : " << Player.second.PlayerName;
		std::cout << ", CurrState : " << (int)Player.second.CurrentState << std::endl;
	}

	auto PlayerVec = Builder.CreateVector(Vec);
	auto ResList = LoginProtocol::CreateS2C_PlayerListResponse(Builder, PlayerVec);

	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::S2C_PlayerListResponse,
		ResList.Union()
	);
	Builder.Finish(SendMsgData);
	SendFlatBufferMessage(ClientSocket, Builder);
}

void Server::ProcessGameReadyRequest(const LoginProtocol::MessageEnvelope* MsgEnvelope, sql::Connection* ClientConn, SOCKET ClientSocket)
{
	//BroadcastPlayerReady
	const LoginProtocol::C2S_GameReadyRequest* ReadyReq = MsgEnvelope->body_as_C2S_GameReadyRequest();
	std::string SessionToken = ReadyReq->session_token()->str();
	bool bIsReady = ReadyReq->is_ready();

	if (PlayerSessionMap.count(SessionToken) > 0)
	{
		PlayerSession& ReadyPlayer = PlayerSessionMap[SessionToken];
		if (bIsReady)
		{
			ReadyPlayer.CurrentState = PlayerState::Ready;
		}
		else
		{
			ReadyPlayer.CurrentState = PlayerState::Lobby;
		}
		
		BroadcastPlayerReady(ReadyPlayer);
	}
	else
	{
		std::cout << "<Bad Request> SessionToken[" << SessionToken << "] is Invalid!!" << std::endl;
	}
}

void Server::BroadcastPlayerJoin(PlayerSession& JoinPlayer)
{
	flatbuffers::FlatBufferBuilder Builder;
	auto UserId = Builder.CreateString(JoinPlayer.UserId);
	auto Nickname = Builder.CreateString(JoinPlayer.PlayerName);
	auto PlayerOffset = LoginProtocol::CreatePlayer(
		Builder,
		UserId,
		Nickname,
		static_cast<LoginProtocol::PlayerState>(JoinPlayer.CurrentState)
	);
	auto InOutOffset = LoginProtocol::CreateS2C_PlayerInOutLobby(
		Builder,
		PlayerOffset,
		true
	);
	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::S2C_PlayerInOutLobby,
		InOutOffset.Union()
	);
	Builder.Finish(SendMsgData);

	for (const auto& Player : PlayerSessionMap)
	{	
		std::cout << "[BroadcastPlayerJoin] User[" << JoinPlayer.UserId << "] Join";
		std::cout << "-> Broadcast UserId[" << Player.second.UserId << "]" << std::endl;

		bool bSend = SendFlatBufferMessage(Player.second.Socket, Builder);
		if (!bSend)
		{
			std::cout << "[BroadcastPlayerJoin] Send Failed User[" << JoinPlayer.UserId << "]";
			std::cout << " Socket[" << Player.second.Socket << "]" << std::endl;
		}
	}
}

void Server::BroadcastPlayerLeave(PlayerSession& LeavePlayer)
{
	flatbuffers::FlatBufferBuilder Builder;
	auto UserId = Builder.CreateString(LeavePlayer.UserId);
	auto Nickname = Builder.CreateString(LeavePlayer.PlayerName);
	auto PlayerOffset = LoginProtocol::CreatePlayer(
		Builder,
		UserId,
		Nickname,
		static_cast<LoginProtocol::PlayerState>(LeavePlayer.CurrentState)
	);
	auto InOutOffset = LoginProtocol::CreateS2C_PlayerInOutLobby(
		Builder,
		PlayerOffset,
		false
	);

	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::S2C_PlayerInOutLobby,
		InOutOffset.Union()
	);
	Builder.Finish(SendMsgData);

	for (const auto& Player : PlayerSessionMap)
	{
		std::cout << "[BroadcastPlayerLeave] User[" << LeavePlayer.UserId << "] Leave";
		std::cout << "-> Broadcast UserId[" << Player.second.UserId << "]" << std::endl;

		SendFlatBufferMessage(Player.second.Socket, Builder);
	}
}

void Server::BroadcastPlayerReady(PlayerSession& ReadyPlayer)
{
	flatbuffers::FlatBufferBuilder Builder;
	auto UserId = Builder.CreateString(ReadyPlayer.UserId);
	auto ReadyOffset = LoginProtocol::CreateS2C_GameReady(
		Builder,
		UserId,
		static_cast<LoginProtocol::PlayerState>(ReadyPlayer.CurrentState)
	);
	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::S2C_GameReady,
		ReadyOffset.Union()
	);
	Builder.Finish(SendMsgData);

	for (const auto& Player : PlayerSessionMap)
	{
		std::cout << "[BroadcastPlayerReady] User[" << ReadyPlayer.UserId << "] Leave";
		std::cout << "-> Broadcast UserId[" << Player.second.UserId << "]" << std::endl;

		SendFlatBufferMessage(Player.second.Socket, Builder);
	}
}

bool Server::RecvAll(SOCKET Sock, char* RecvBuff, size_t RecvLen)
{
	size_t Received = 0;
	while (Received < RecvLen)
	{
		int RecvBytes = recv(Sock, RecvBuff + Received, int(RecvLen - Received), MSG_WAITALL);
		if (RecvBytes <= 0)
		{
			if (RecvBytes == 0)
			{
				std::cout << "[RecvAll] Client disconnected." << std::endl;
			}
			else
			{
				std::cerr << "RecvAll failed: " << WSAGetLastError() << std::endl;
			}
			return false;
		}

		Received += RecvBytes;
	}
	return true;
}

bool Server::SendAll(SOCKET Sock, char* SendBuff, size_t SendLen)
{
	size_t TotalSentBytes = 0;
	while (TotalSentBytes < SendLen)
	{
		int SentBytes = send(Sock, SendBuff + TotalSentBytes, int(SendLen - TotalSentBytes), 0);
		if (SentBytes < 0)
		{
			std::cerr << "SendAll failed: " << WSAGetLastError() << std::endl;
			return false;
		}
		TotalSentBytes += SentBytes;
	}
	return true;
}

// 메시지 전송 함수
bool Server::SendFlatBufferMessage(SOCKET Sock, flatbuffers::FlatBufferBuilder& Builder)
{
	// 1. 메시지 길이 (4바이트) 전송
	uint32_t MessageSize = Builder.GetSize();
	uint32_t NetworkMessageSize = htonl(MessageSize); // 네트워크 바이트 순서로 변환
	if (SendAll(Sock, (char*)&NetworkMessageSize, sizeof(NetworkMessageSize)) == false)
	{
		std::cerr << "Send message size failed." << std::endl;
		return false;
	}

	// 2. 실제 메시지 데이터 전송
	if (SendAll(Sock, (char*)Builder.GetBufferPointer(), MessageSize) == false)
	{
		std::cerr << "Send message data failed." << std::endl;
		return false;
	}
	return true;
}

// 메시지 수신 함수
bool Server::ReceiveFlatBufferMessage(SOCKET Sock, std::vector<char>& RecvBuf, uint32_t& outMessageSize)
{
	// 1. 메시지 길이 (4바이트) 수신
	uint32_t networkMessageSize;
	bool bRecv = RecvAll(Sock, (char*)&networkMessageSize, sizeof(networkMessageSize));
	if (bRecv == false)
	{
		std::cerr << "Receive message size failed." << std::endl;
		return false;
	}
	outMessageSize = ntohl(networkMessageSize); // 호스트 바이트 순서로 변환

	// 2. 실제 메시지 데이터 수신
	RecvBuf.resize(outMessageSize);
	bRecv = RecvAll(Sock, (char*)RecvBuf.data(), outMessageSize);
	if (bRecv == false)
	{
		std::cerr << "Receive message data failed." << std::endl;
		return false;
	}
	return true;
}

void Server::CleanUpClientSession(SOCKET ClientSocket)
{
	std::string SessionToken;

	{
		std::shared_lock<std::shared_mutex> Lock(SessionMutex);
		auto it = TokenBySocketMap.find(ClientSocket);
		if(it == TokenBySocketMap.end())
		{
			// 이미 종료되거나 정리된 세션
			return;
		}
		SessionToken = it->second;
	}

	std::unique_lock<std::shared_mutex> Lock(SessionMutex);

	PlayerSession LeavePlayer = PlayerSessionMap[SessionToken];
	int32_t PlayerId = LeavePlayer.DbId;

	// 3개 맵에서 모두 삭제
	TokenBySocketMap.erase(ClientSocket);
	TokenByPlayerIdMap.erase(PlayerId);
	PlayerSessionMap.erase(SessionToken);

	// 다른 클라이언트들에게 나간다고 브로드캐스트
	BroadcastPlayerLeave(LeavePlayer);

	std::cout << "### Client Connection Closed [" << ClientSocket << "]" << std::endl;
}
