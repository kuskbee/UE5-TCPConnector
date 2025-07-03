#pragma once

#include <iostream>
#include <objbase.h>
#include <string>
#include <sstream>
#include <iomanip>

namespace util
{

class UuidHelper
{
public:
    // UUID�� ���ڿ��� ��ȯ
    std::string UuidToString(const UUID& uuid)
    {
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0');

        ss << std::setw(8) << uuid.Data1 << "-";
        ss << std::setw(4) << uuid.Data2 << "-";
        ss << std::setw(4) << uuid.Data3 << "-";

        ss << std::setw(2) << static_cast<int>(uuid.Data4[0]);
        ss << std::setw(2) << static_cast<int>(uuid.Data4[1]) << "-";
        ss << std::setw(2) << static_cast<int>(uuid.Data4[2]);
        ss << std::setw(2) << static_cast<int>(uuid.Data4[3]);
        ss << std::setw(2) << static_cast<int>(uuid.Data4[4]);
        ss << std::setw(2) << static_cast<int>(uuid.Data4[5]);
        ss << std::setw(2) << static_cast<int>(uuid.Data4[6]);
        ss << std::setw(2) << static_cast<int>(uuid.Data4[7]);

        return ss.str();
    }

    // UUID ����
    std::string GenerateSessionToken()
    {
        UUID uuid;
        // UUID ����. ���� �� RPC_S_OK ��ȯ
        RPC_STATUS status = UuidCreate(&uuid);

        // ���� ���� ����
        if (status != RPC_S_OK)
        {
            std::cout << "[ERROR] Failed to Generate SessionToken" << std::endl;
            return "";
        }

        return UuidToString(uuid);
    }


};

} // namespace util
