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
		ReceiveFlatBufferMessage(RecvBuf, MessageSize);

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
	HeaderBuffer.AddZeroed();
	
	int ReadBytes = 0;
	bool bRecv = Socket->Recv(HeaderBuffer.GetData(), HeaderSize, ReadBytes, ESocketReceiveFlags::WaitAll);
	if (bRecv == false)
	{
		UE_LOG(LogTemp, Error, TEXT("Receive message size failed."));
		return false;
	}
	outMessageSize = ntohl(networkMessageSize); // 호스트 바이트 순서로 변환

	// 2. 실제 메시지 데이터 수신

	RecvBuf.SetNumUninitialized(outMessageSize);
	bRecv = Socket->Recv(RecvBuf.GetData(), outMessageSize, ReadBytes, ESocketReceiveFlags::WaitAll);
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
	}

	switch (MsgEnvelope->body_type())
	{
	case LoginProtocol::Payload_S2C_LoginResponse:
	{
		ProcessLoginResponse(MsgEnvelope);
		break;
	}
	case LoginProtocol::Payload_S2C_SignUpResponse:
	{
		ProcessSignUpResponse(MsgEnvelope);
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
	const uint16 ErrCode = LoginRes->error_code();

	const char* Utf8Nickname = LoginRes->nickname()->c_str();
	const FString Nickname = FString(UTF8_TO_TCHAR(Utf8Nickname));

	const char* Utf8Token = LoginRes->session_token()->c_str();
	const FString SessionToken = FString(UTF8_TO_TCHAR(Utf8Token));


	OnLoginResponseDelegate.Broadcast(ErrCode, Nickname, SessionToken);
}

void ULoginClientSubsystem::ProcessSignUpResponse(const LoginProtocol::MessageEnvelope* MsgEnvelope)
{
	const LoginProtocol::S2C_SignUpResponse* SignUpRes = MsgEnvelope->body_as_S2C_SignUpResponse();
	const uint16 ErrCode = SignUpRes->error_code();
	
	OnSignUpResponseDelegate.Broadcast(ErrCode);
}
