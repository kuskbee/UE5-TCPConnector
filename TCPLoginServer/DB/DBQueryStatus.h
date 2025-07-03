#pragma once
#include <cstdint>

enum class SignUpStatus : uint16_t
{
	Success,
	DuplicateUserId,
	DuplicateNickname,
	OtherError
};

enum class LoginStatus : uint16_t
{
	Success,
	UserIdNotFound,
	InvalidPassword,
	OtherError
};