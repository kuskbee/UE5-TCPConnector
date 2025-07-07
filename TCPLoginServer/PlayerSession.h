#pragma once
#include <string>
#include <cstdint>

#include <WinSock2.h>

enum class PlayerState : uint8_t
{
    Lobby,     // �κ� ������ ����
    Ready,     // �κ񿡼� �غ� �Ϸ��� ����
    InGame     // ���ӿ� ������ ����
};

struct PlayerSession
{
    int32_t DbId;                 // DB�� ���� ID
    std::string UserId;           // ���� ID
    std::string PlayerName;       // �÷��̾� �г���
    SOCKET Socket;              // �ش� Ŭ���̾�Ʈ�� ���� ����
    PlayerState CurrentState;    // ���� �÷��̾� ���� (Lobby, Ready ��)
};