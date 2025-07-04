#pragma once
#include <chrono>
#include <cstdint>

uint64_t GetTimeStamp()
{
	// �ý��� �ð踦 UTC epoch(1970-01-01) �������� �о
	// �и��� ���� ������ ��ȯ
	using namespace std::chrono;
	auto now = system_clock::now();
	auto ms = duration_cast<milliseconds>(now.time_since_epoch());
	return static_cast<uint64_t>(ms.count());
}
