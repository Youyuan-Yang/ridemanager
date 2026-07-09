#pragma once

#include "common/IMUData.h"

namespace imu {

class MadgwickAHRS {
public:
    explicit MadgwickAHRS(double beta);

    void setBeta(double beta);
    void reset();
    void update(const Vector3& accelerationG, const Vector3& gyroscopeRadPerSec, double dtSeconds);

    Quaternion quaternion() const;
    EulerAngles eulerDegrees() const;

private:
    static double inverseSqrt(double value);
    static double radiansToDegrees(double radians);

    double beta_{0.1};
    Quaternion q_{};
};

}  // namespace imu
