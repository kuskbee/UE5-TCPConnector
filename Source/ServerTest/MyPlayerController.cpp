// Fill out your copyright notice in the Description page of Project Settings.


#include "MyPlayerController.h"
#include "LoginClientSubsystem.h"


void AMyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (UGameInstance* GI = GetGameInstance())
    {
        if (ULoginClientSubsystem* Subsys = GI->GetSubsystem<ULoginClientSubsystem>())
        {
            // Dynamic Multicast Delegate ¹ÙÀÎµù
            Subsys->OnStartGameDelegate.AddDynamic(this, &AMyPlayerController::OnStartGame);
        }
    }
}

void AMyPlayerController::OnStartGame(FString DediIp, int DediPort)
{
    FString ServerAddress = FString::Printf(TEXT("%s:%d"), *DediIp, DediPort);
    
    UE_LOG(LogTemp, Warning, TEXT("Server Address : [%s]"), *ServerAddress);
    ClientTravel(ServerAddress, TRAVEL_Absolute);
}

