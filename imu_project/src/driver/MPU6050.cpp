#include "driver/MPU6050.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <utility>

#ifdef __linux__
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#endif

#include "common/Logger.h"

namespace imu {

namespace {

[[maybe_unused]] constexpr uint8_t kRegisterSampleRateDivider = 0x19;
[[maybe_unused]] constexpr uint8_t kRegisterConfig = 0x1A;
[[maybe_unused]] constexpr uint8_t kRegisterGyroConfig = 0x1B;
[[maybe_unused]] constexpr uint8_t kRegisterAccelConfig = 0x1C;
[[maybe_unused]] constexpr uint8_t kRegisterAccelXoutH = 0x3B;
[[maybe_unused]] constexpr uint8_t kRegisterPowerManagement1 = 0x6B;
[[maybe_unused]] constexpr uint8_t kRegisterWhoAmI = 0x75;

[[maybe_unused]] constexpr uint8_t kWakeUp = 0x00;
[[maybe_unused]] constexpr uint8_t kSampleRateDivider100Hz = 0x09;
[[maybe_unused]] constexpr uint8_t kDlpfBandwidth42Hz = 0x03;
[[maybe_unused]] constexpr uint8_t kGyroRange250Dps = 0x00;
[[maybe_unused]] constexpr uint8_t kAccelRange2g = 0x00;

constexpr double kAccelerationScaleLsbPerG = 16384.0;
constexpr double kGyroscopeScaleLsbPerDps = 131.0;
constexpr double kTemperatureScale = 340.0;
constexpr double kTemperatureOffsetC = 36.53;

std::string systemError(const std::string& message) {
    return message + ": " + std::strerror(errno);
}

[[maybe_unused]] std::string hexByte(uint8_t value) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(value);
    return oss.str();
}

}  // namespace

MPU6050::MPU6050(std::string i2cDevice, int address)
    : i2cDevice_(std::move(i2cDevice)), address_(address) {}

MPU6050::~MPU6050() {
    closeDevice();
}

void MPU6050::init() {
#ifndef __linux__
    throw std::runtime_error("MPU6050 requires Linux I2C support");
#else
    closeDevice();

    Logger::info("I2C device: " + i2cDevice_);

    fd_ = ::open(i2cDevice_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        throw std::runtime_error(systemError("failed to open " + i2cDevice_));
    }

    if (::ioctl(fd_, I2C_SLAVE, address_) < 0) {
        closeDevice();
        throw std::runtime_error(systemError("failed to set MPU6050 I2C slave address"));
    }

    const uint8_t whoAmI = readRegister(kRegisterWhoAmI);
    if (whoAmI != 0x68) {
        closeDevice();
        throw std::runtime_error("unexpected MPU6050 WHO_AM_I value: " + hexByte(whoAmI));
    }

    Logger::info("MPU6050 detected: " + hexByte(whoAmI));

    writeRegister(kRegisterPowerManagement1, kWakeUp);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    writeRegister(kRegisterSampleRateDivider, kSampleRateDivider100Hz);
    writeRegister(kRegisterConfig, kDlpfBandwidth42Hz);
    writeRegister(kRegisterGyroConfig, kGyroRange250Dps);
    writeRegister(kRegisterAccelConfig, kAccelRange2g);

    initialized_ = true;
    Logger::info("IMU initialized");
#endif
}

RawIMUData MPU6050::read() {
    if (!initialized_) {
        throw std::runtime_error("MPU6050 read called before init");
    }

    std::array<uint8_t, 14> buffer{};
    readRegisters(kRegisterAccelXoutH, buffer.data(), buffer.size());

    const int16_t rawAccX = toSigned16(buffer[0], buffer[1]);
    const int16_t rawAccY = toSigned16(buffer[2], buffer[3]);
    const int16_t rawAccZ = toSigned16(buffer[4], buffer[5]);
    const int16_t rawTemp = toSigned16(buffer[6], buffer[7]);
    const int16_t rawGyroX = toSigned16(buffer[8], buffer[9]);
    const int16_t rawGyroY = toSigned16(buffer[10], buffer[11]);
    const int16_t rawGyroZ = toSigned16(buffer[12], buffer[13]);

    RawIMUData data;
    data.observedAt = std::chrono::system_clock::now();
    data.accelerationG = Vector3{
        static_cast<double>(rawAccX) / kAccelerationScaleLsbPerG,
        static_cast<double>(rawAccY) / kAccelerationScaleLsbPerG,
        static_cast<double>(rawAccZ) / kAccelerationScaleLsbPerG,
    };
    data.gyroscopeDps = Vector3{
        static_cast<double>(rawGyroX) / kGyroscopeScaleLsbPerDps,
        static_cast<double>(rawGyroY) / kGyroscopeScaleLsbPerDps,
        static_cast<double>(rawGyroZ) / kGyroscopeScaleLsbPerDps,
    };
    data.temperatureC = static_cast<double>(rawTemp) / kTemperatureScale + kTemperatureOffsetC;
    data.status = SensorStatus::Ok;
    return data;
}

int16_t MPU6050::toSigned16(uint8_t high, uint8_t low) {
    const uint16_t combined = static_cast<uint16_t>((static_cast<uint16_t>(high) << 8U) | low);
    return static_cast<int16_t>(combined);
}

void MPU6050::closeDevice() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    initialized_ = false;
}

uint8_t MPU6050::readRegister(uint8_t reg) {
    uint8_t value = 0;
    readRegisters(reg, &value, 1);
    return value;
}

void MPU6050::readRegisters(uint8_t startReg, uint8_t* buffer, std::size_t length) {
    if (::write(fd_, &startReg, 1) != 1) {
        throw std::runtime_error(systemError("failed to write MPU6050 register address"));
    }

    const ssize_t bytesRead = ::read(fd_, buffer, length);
    if (bytesRead < 0) {
        throw std::runtime_error(systemError("failed to read MPU6050 registers"));
    }

    if (static_cast<std::size_t>(bytesRead) != length) {
        throw std::runtime_error("short MPU6050 I2C read");
    }
}

void MPU6050::writeRegister(uint8_t reg, uint8_t value) {
    const std::array<uint8_t, 2> buffer{reg, value};
    if (::write(fd_, buffer.data(), buffer.size()) != static_cast<ssize_t>(buffer.size())) {
        throw std::runtime_error(systemError("failed to write MPU6050 register"));
    }
}

}  // namespace imu
