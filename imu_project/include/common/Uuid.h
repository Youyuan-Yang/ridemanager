#pragma once

#include <array>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace imu {

inline std::string generateUuidV4() {
    std::array<unsigned char, 16> bytes{};
    std::random_device randomDevice;

    for (auto& byte : bytes) {
        byte = static_cast<unsigned char>(randomDevice() & 0xFF);
    }

    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            oss << '-';
        }
    }
    return oss.str();
}

inline bool isValidUuid(const std::string& value) {
    if (value.size() != 36) {
        return false;
    }

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (value[i] != '-') {
                return false;
            }
            continue;
        }

        if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }

    return true;
}

}  // namespace imu
