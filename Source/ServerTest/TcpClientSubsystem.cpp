// Fill out your copyright notice in the Description page of Project Settings.


#include "TcpClientSubsystem.h"
#include "Networking.h"
//#include "Sockets.h"
#include "SocketSubsystem.h"
//#include "Common/TcpSocketBuilder.h"

void UTcpClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Socket = nullptr;
}

void UTcpClientSubsystem::Deinitialize()
{
	DisconnectFromServer();
	Super::Deinitialize();
}

void UTcpClientSubsystem::ConnectToServer(const FString& IpAddress, int32 Port)
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
	GetWorld()->GetTimerManager().SetTimer(RecvDataTimerHandle, this, &UTcpClientSubsystem::RecvData, 0.1f, true);
}

void UTcpClientSubsystem::DisconnectFromServer()
{
	GetWorld()->GetTimerManager().ClearTimer(RecvDataTimerHandle);

	if (Socket)
	{
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

		SocketSubsystem->DestroySocket(Socket);
		Socket = nullptr;
		UE_LOG(LogTemp, Log, TEXT("Disconnected from server."));
	}
}

void UTcpClientSubsystem::RecvData()
{
	if (!Socket) return;

	TArray<uint8> RecvData;
	uint32 Size;
	while (Socket && Socket->HasPendingData(Size))
	{
		RecvData.SetNumUninitialized(FMath::Min(Size, 65507u));
		int32 Read = 0;
		Socket->Recv(RecvData.GetData(), RecvData.Num(), Read);

		if (Read > 0)
		{
			FString Message = FString(Read, (const char*)RecvData.GetData());
			UE_LOG(LogTemp, Log, TEXT("Received message from server : %s"), *Message);

			if (Message.Contains(TEXT("Hell")))
			{
				UE_LOG(LogTemp, Log, TEXT("Received 'Hell', disconnecting."));
				DisconnectFromServer();
			}
		}
	}
}
