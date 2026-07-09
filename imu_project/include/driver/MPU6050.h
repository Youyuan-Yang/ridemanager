#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "common/IMUData.h"

namespace imu {

class MPU6050 {
public:
    MPU6050(std::string i2cDevice, int address);
    ~MPU6050();

    MPU6050(const MPU6050&) = delete;
    MPU6050& operator=(const MPU6050&) = delete;
    MPU6050(MPU6050&&) = delete;
    MPU6050& operator=(MPU6050&&) = delete;

    void init();
    RawIMUData read();

    const std::string& i2cDevice() const {
        return i2cDevice_;
    }

    int address() const {
        return address_;
    }

private:
    static int16_t toSigned16(uint8_t high, uint8_t low);

    void closeDevice() noexcept;
    uint8_t readRegister(uint8_t reg);
    void readRegisters(uint8_t startReg, uint8_t* buffer, std::size_t length);
    void writeRegister(uint8_t reg, uint8_t value);

    std::string i2cDevice_;
    int address_;
    int fd_{-1};
    bool initialized_{false};
};

}  // namespace imu
