// Fill out your copyright notice in the Description page of Project Settings.


#include "LoginClientSubsystem.h"

//WinSock (hton, ntoh)
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include "Networking.h"
#include "SocketSubsystem.h"
//#include "Sockets.h"
//#include "Common/TcpSocketBuilder.h"

#include "UObject/EnumProperty.h"

#include "Utils.h"

void ULoginClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Socket = nullptr;
}

void ULoginClientSubsystem::Deinitialize()
{
	DisconnectFromServer();
	Super::Deinitialize();
}

void ULoginClientSubsystem::ConnectToServer(const FString& IpAddress, int32 Port)
{
	if (Socket)
	{
		UE_LOG(LogTemp, Warning, TEXT("Already connected."));
		return;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get Socket Subsystem"));
		return;
	}

	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	FIPv4Address IPv4Addr;
	FIPv4Address::Parse(IpAddress, IPv4Addr);
	Addr->SetIp(IPv4Addr.Value);
	Addr->SetPort(Port);

	Socket = FTcpSocketBuilder(TEXT("LoginClientSocket")).AsNonBlocking().Build();

	if (!Socket || !Socket->Connect(*Addr))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to connect to server: %s:%d"), *IpAddress, Port);

		if (Socket)
		{
			SocketSubsystem->DestroySocket(Socket);
			Socket = nullptr;
		}
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Connected to Server: %s:%d"), *IpAddress, Port);

	// 주기적으로 데이터가 들어왔는지 체크하기 위한 타이머 시작
	GetWorld()->GetTimerManager().SetTimer(NetworkTimerHandle, this, &ULoginClientSubsystem::NetworkPolling, 0.1f, true);
}

void ULoginClientSubsystem::DisconnectFromServer()
{
	GetWorld()->GetTimerManager().ClearTimer(NetworkTimerHandle);

	if (Socket)
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

		SocketSubsystem->DestroySocket(Socket);
		Socket = nullptr;
		UE_LOG(LogTemp, Log, TEXT("Disconnected from server."));
	}
}

bool ULoginClientSubsystem::IsConnect()
{
	if (Socket && (Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected))
	{
		return true;
	}

	return false;
}

void ULoginClientSubsystem::NetworkPolling()
{
	if (!Socket) return;

	constexpr size_t READ_CHUNK = 64 * 1024; // 64KB
	TArray<uint8_t> RecvBuf;
	RecvBuf.Reserve(READ_CHUNK);

	uint32 Size;
	while (Socket && Socket->HasPendingData(Size))
	{
		uint32_t MessageSize = 0;
		bool bRecv = ReceiveFlatBufferMessage(RecvBuf, MessageSize);
		if (false == bRecv)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed ReceiveFlatBufferMessasge."));
			break;
		}

		// FlatBuffer 검증
		flatbuffers::Verifier verifier(
			reinterpret_cast<const uint8_t*>(RecvBuf.GetData()), MessageSize);

		if (!LoginProtocol::VerifyMessageEnvelopeBuffer(verifier))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid FlatBuffer received"));
			break;
		}

		// 메시지 디스패치
		ProcessPacket(RecvBuf);
	}
}

bool ULoginClientSubsystem::SendFlatBufferMessage(flatbuffers::FlatBufferBuilder& Builder)
{
	uint32 MessageSize = Builder.GetSize();
	uint32 NetworkMessageSize = htonl(MessageSize);
	int SentBytes = 0;
	if (false == Socket->Send((uint8*)&NetworkMessageSize, sizeof(NetworkMessageSize), SentBytes))
	{
		UE_LOG(LogTemp, Error, TEXT("Send message size failed."));
		return false;
	}

	SentBytes = 0;
	if (false == Socket->Send((uint8*)Builder.GetBufferPointer(), MessageSize, SentBytes))
	{
		UE_LOG(LogTemp, Error, TEXT("Send message data failed."));
		return false;
	}

	return true;
}

bool ULoginClientSubsystem::ReceiveFlatBufferMessage(TArray<uint8_t>& RecvBuf, uint32_t& outMessageSize)
{
	// 1. 메시지 길이 (4바이트) 수신
	uint32_t networkMessageSize = 0;
	TArray<uint8_t> HeaderBuffer;
	int32 HeaderSize = (int32)sizeof(uint32_t);
	HeaderBuffer.AddZeroed(HeaderSize);
	
	int ReadBytes = 0;
	bool bRecv = Socket->Recv(HeaderBuffer.GetData(), HeaderSize, ReadBytes);
	if (bRecv == false)
	{
		UE_LOG(LogTemp, Error, TEXT("Receive message size failed."));
		return false;
	}

	FMemory::Memcpy(&networkMessageSize, HeaderBuffer.GetData(), sizeof(uint32_t));
	outMessageSize = ntohl(networkMessageSize); // 호스트 바이트 순서로 변환

	// 2. 실제 메시지 데이터 수신

	RecvBuf.SetNumUninitialized(outMessageSize);
	bRecv = Socket->Recv(RecvBuf.GetData(), outMessageSize, ReadBytes);
	if (bRecv == false)
	{
		UE_LOG(LogTemp, Error, TEXT("Receive message data failed."));
		return false;
	}
	return true;
}

void ULoginClientSubsystem::ProcessPacket(TArray<uint8_t>& RecvBuf)
{
	const LoginProtocol::MessageEnvelope* MsgEnvelope = LoginProtocol::GetMessageEnvelope(RecvBuf.GetData());
	if (!MsgEnvelope)
	{
		UE_LOG(LogTemp, Error, TEXT("[ProcessPacket] Failed to GetMessageEnvelope"));
		return;
	}

	switch (MsgEnvelope->body_type())
	{
	case LoginProtocol::Payload::S2C_LoginResponse:
	{
		ProcessLoginResponse(MsgEnvelope);
		break;
	}
	case LoginProtocol::Payload::S2C_SignUpResponse:
	{
		ProcessSignUpResponse(MsgEnvelope);
		break;
	}
	case LoginProtocol::Payload::S2C_PlayerListResponse:
	{
		ProcessPlayerListResponse(MsgEnvelope);
		break;
	}
	case LoginProtocol::Payload::S2C_PlayerInOutLobby:
	{
		ProcessPlayerInOutLobby(MsgEnvelope);
		break;
	}
	case LoginProtocol::Payload::S2C_PlayerChangeState:
	{
		ProcessPlayerChangeState(MsgEnvelope);
		break;
	}
	default:
		UE_LOG(LogTemp, Error, TEXT("Unknown payload type [%d]"), static_cast<uint8_t>(MsgEnvelope->body_type()));
		break;
	}
}

void ULoginClientSubsystem::ProcessLoginResponse(const LoginProtocol::MessageEnvelope* MsgEnvelope)
{
	const LoginProtocol::S2C_LoginResponse* LoginRes = MsgEnvelope->body_as_S2C_LoginResponse();
	const ELoginServerErrorCode ErrCode = static_cast<ELoginServerErrorCode>(LoginRes->error_code());

	const char* Utf8Nickname = LoginRes->nickname()->c_str();
	const FString Nickname = FString(UTF8_TO_TCHAR(Utf8Nickname));

	const char* Utf8Token = LoginRes->session_token()->c_str();
	const FString SessionToken = FString(UTF8_TO_TCHAR(Utf8Token));


	OnLoginResponseDelegate.Broadcast(ErrCode, Nickname, SessionToken);
}

void ULoginClientSubsystem::ProcessSignUpResponse(const LoginProtocol::MessageEnvelope* MsgEnvelope)
{
	const LoginProtocol::S2C_SignUpResponse* SignUpRes = MsgEnvelope->body_as_S2C_SignUpResponse();
	const ELoginServerErrorCode ErrCode = static_cast<ELoginServerErrorCode>(SignUpRes->error_code());
	
	OnSignUpResponseDelegate.Broadcast(ErrCode);
}

void ULoginClientSubsystem::ProcessPlayerListResponse(const LoginProtocol::MessageEnvelope* MsgEnvelope)
{
	const LoginProtocol::S2C_PlayerListResponse* PlayerListRes = MsgEnvelope->body_as_S2C_PlayerListResponse();

	TArray<FPlayerInfo> PlayerInfos;

	UE_LOG(LogTemp, Warning, TEXT("[PlayerList] Received Player List, Num : %d"), PlayerListRes->players()->size());

	for (auto Player : *PlayerListRes->players())
	{
		const char* Utf8UserId = Player->user_id()->c_str();
		const FString UserId = FString(UTF8_TO_TCHAR(Utf8UserId));

		const char* Utf8Nickname = Player->nickname()->c_str();
		const FString Nickname = FString(UTF8_TO_TCHAR(Utf8Nickname));

		EPlayerState PlyState = static_cast<EPlayerState>(Player->state());

		PlayerInfos.Add(FPlayerInfo(UserId, Nickname, PlyState));

		UE_LOG(LogTemp, Warning, TEXT("[PlayerList] UserId : %s, NickName : %s, State : %s"), 
			*UserId, *Nickname, *StaticEnum<EPlayerState>()->GetNameStringByValue(static_cast<int64>(PlyState)));
	}

	UE_LOG(LogTemp, Warning, TEXT("[PlayerList] Received Player List, Num : %d"), PlayerInfos.Num());

	OnPlayerListResponseDelegate.Broadcast(PlayerInfos);
}

void ULoginClientSubsystem::ProcessPlayerInOutLobby(const LoginProtocol::MessageEnvelope* MsgEnvelope)
{
	const LoginProtocol::S2C_PlayerInOutLobby* PlayerInOutRes = MsgEnvelope->body_as_S2C_PlayerInOutLobby();

	const char* Utf8UserId = PlayerInOutRes->player()->user_id()->c_str();
	const FString UserId = FString(UTF8_TO_TCHAR(Utf8UserId));

	const char* Utf8Nickname = PlayerInOutRes->player()->nickname()->c_str();
	const FString Nickname = FString(UTF8_TO_TCHAR(Utf8Nickname));

	EPlayerState PlyState = static_cast<EPlayerState>(PlayerInOutRes->player()->state());

	bool bIsJoin = PlayerInOutRes->is_join();

	UE_LOG(LogTemp, Warning, TEXT("[ProcessPlayerInOutLobby] %s IsJoin [%d]"), *Nickname, bIsJoin);

	OnPlayerInOutLobbyDelegate.Broadcast(FPlayerInfo(UserId, Nickname, PlyState), bIsJoin);
}

void ULoginClientSubsystem::ProcessPlayerChangeState(const LoginProtocol::MessageEnvelope* MsgEnvelope)
{
	const LoginProtocol::S2C_PlayerChangeState* ChangeStateRes = MsgEnvelope->body_as_S2C_PlayerChangeState();

	const char* Utf8UserId = ChangeStateRes->user_id()->c_str();
	const FString UserId = FString(UTF8_TO_TCHAR(Utf8UserId));

	EPlayerState PlyState = static_cast<EPlayerState>(ChangeStateRes->state());

	OnPlayerChangeStateDelegate.Broadcast(UserId, PlyState);
}

void ULoginClientSubsystem::SendLoginRequest(const FString& UserId, const FString& Password)
{
	std::string UserIdCharBuf = TCHAR_TO_UTF8(*UserId);
	std::string PasswordCharBuf = TCHAR_TO_UTF8(*Password);

	flatbuffers::FlatBufferBuilder Builder;
	auto UserIdOffset = Builder.CreateString(UserIdCharBuf);
	auto PasswordOffset = Builder.CreateString(PasswordCharBuf);
	auto BodyOffset = LoginProtocol::CreateC2S_LoginRequest(
		Builder,
		UserIdOffset,
		PasswordOffset
	);

	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::C2S_LoginRequest,
		BodyOffset.Union()
	);
	Builder.Finish(SendMsgData);
	SendFlatBufferMessage(Builder);
}

void ULoginClientSubsystem::SendSignUpRequest(const FString& UserId, const FString& Password, const FString& Nickname)
{
	std::string UserIdCharBuf = TCHAR_TO_UTF8(*UserId);
	std::string PasswordCharBuf = TCHAR_TO_UTF8(*Password);
	std::string NicknameCharBuf = TCHAR_TO_UTF8(*Nickname);

	flatbuffers::FlatBufferBuilder Builder;
	auto UserIdOffset = Builder.CreateString(UserIdCharBuf);
	auto PasswordOffset = Builder.CreateString(PasswordCharBuf);
	auto NicknameOffset = Builder.CreateString(NicknameCharBuf);
	auto BodyOffset = LoginProtocol::CreateC2S_SignUpRequest(
		Builder,
		UserIdOffset,
		PasswordOffset,
		NicknameOffset
	);

	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::C2S_SignUpRequest,
		BodyOffset.Union()
	);
	Builder.Finish(SendMsgData);
	SendFlatBufferMessage(Builder);
}

void ULoginClientSubsystem::SendPlayerListRequest()
{
	flatbuffers::FlatBufferBuilder Builder;
	auto BodyOffset = LoginProtocol::CreateC2S_PlayerListRequest(Builder);
	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::C2S_PlayerListRequest,
		BodyOffset.Union()
	);
	Builder.Finish(SendMsgData);
	SendFlatBufferMessage(Builder);
}

void ULoginClientSubsystem::SendPlayerReadyRequest(const FString& SessionToken, const bool bIsReady)
{
	std::string TokenCharBuf = TCHAR_TO_UTF8(*SessionToken);
	
	flatbuffers::FlatBufferBuilder Builder;
	auto TokenOffset = Builder.CreateString(TokenCharBuf);
	auto BodyOffset = LoginProtocol::CreateC2S_GameReadyRequest(Builder,
		TokenOffset,
		bIsReady
	);
	auto SendMsgData = LoginProtocol::CreateMessageEnvelope(
		Builder,
		GetTimeStamp(),
		LoginProtocol::Payload::C2S_GameReadyRequest,
		BodyOffset.Union()
	);
	Builder.Finish(SendMsgData);
	SendFlatBufferMessage(Builder);
}
