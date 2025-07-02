// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TcpClientSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class SERVERTEST_API UTcpClientSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "TCP")
	void ConnectToServer(const FString& IpAddress = "127.0.0.1", int32 Port = 8881);
	
	UFUNCTION(BlueprintCallable, Category = "TCP")
	void DisconnectFromServer();

private:
	void RecvData();

	TObjectPtr<FSocket> Socket;
	FTimerHandle RecvDataTimerHandle;
	
};
