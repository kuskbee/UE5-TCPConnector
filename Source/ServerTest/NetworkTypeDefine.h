#pragma once

UENUM(BlueprintType)
enum class ELoginServerErrorCode : uint8
{
	ELS_Success UMETA(DisplayName = "Success"),
	ELS_Login_Fail_UserNotFound  UMETA(DisplayName = "Login_Fail_UserNotFound"),
	ELS_Login_Fail_InvalidPassword UMETA(DisplayName = "Login_Fail_InvalidPassword"),
	ELS_SignUp_Fail_UsernameExists UMETA(DisplayName = "SignUp_Fail_UsernameExists"),
	ELS_SignUp_Fail_NicknameExists UMETA(DisplayName = "SignUp_Fail_NicknameExists"),
	ELS_Auth_Fail_InvalidToken UMETA(DisplayName = "Auth_Fail_InvalidToken"),
	ELS_InvalidRequest UMETA(DisplayName = "InvalidRequest"),
	ELS_ServerError UMETA(DisplayName = "ServerError"),
};

UENUM(BlueprintType)
enum class EPlayerState : uint8
{
	EPS_Lobby UMETA(DisplayName = "Lobby"),     // �κ� ������ ����
	EPS_Ready UMETA(DisplayName = "Ready"),     // �κ񿡼� �غ� �Ϸ��� ����
	EPS_InGame UMETA(DisplayName = "InGame")     // ���ӿ� ������ ����
};
