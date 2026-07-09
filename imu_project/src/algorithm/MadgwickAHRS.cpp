#include "algorithm/MadgwickAHRS.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace imu {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSmallNumber = 1e-12;
}

MadgwickAHRS::MadgwickAHRS(double beta) {
    setBeta(beta);
    reset();
}

void MadgwickAHRS::setBeta(double beta) {
    if (beta <= 0.0) {
        throw std::invalid_argument("Madgwick beta must be positive");
    }
    beta_ = beta;
}

void MadgwickAHRS::reset() {
    q_ = Quaternion{1.0, 0.0, 0.0, 0.0};
}

void MadgwickAHRS::update(
    const Vector3& accelerationG,
    const Vector3& gyroscopeRadPerSec,
    double dtSeconds) {
    if (dtSeconds <= 0.0) {
        return;
    }

    double q0 = q_.q0;
    double q1 = q_.q1;
    double q2 = q_.q2;
    double q3 = q_.q3;

    const double gx = gyroscopeRadPerSec.x;
    const double gy = gyroscopeRadPerSec.y;
    const double gz = gyroscopeRadPerSec.z;

    double qDot0 = 0.5 * (-q1 * gx - q2 * gy - q3 * gz);
    double qDot1 = 0.5 * (q0 * gx + q2 * gz - q3 * gy);
    double qDot2 = 0.5 * (q0 * gy - q1 * gz + q3 * gx);
    double qDot3 = 0.5 * (q0 * gz + q1 * gy - q2 * gx);

    double ax = accelerationG.x;
    double ay = accelerationG.y;
    double az = accelerationG.z;
    const double accelNormSquared = ax * ax + ay * ay + az * az;

    if (accelNormSquared > kSmallNumber) {
        const double recipNorm = inverseSqrt(accelNormSquared);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        const double twoQ0 = 2.0 * q0;
        const double twoQ1 = 2.0 * q1;
        const double twoQ2 = 2.0 * q2;
        const double twoQ3 = 2.0 * q3;
        const double fourQ0 = 4.0 * q0;
        const double fourQ1 = 4.0 * q1;
        const double fourQ2 = 4.0 * q2;
        const double eightQ1 = 8.0 * q1;
        const double eightQ2 = 8.0 * q2;
        const double q0q0 = q0 * q0;
        const double q1q1 = q1 * q1;
        const double q2q2 = q2 * q2;
        const double q3q3 = q3 * q3;

        double s0 = fourQ0 * q2q2 + twoQ2 * ax + fourQ0 * q1q1 - twoQ1 * ay;
        double s1 = fourQ1 * q3q3 - twoQ3 * ax + 4.0 * q0q0 * q1 -
                    twoQ0 * ay - fourQ1 + eightQ1 * q1q1 + eightQ1 * q2q2 +
                    fourQ1 * az;
        double s2 = 4.0 * q0q0 * q2 + twoQ0 * ax + fourQ2 * q3q3 -
                    twoQ3 * ay - fourQ2 + eightQ2 * q1q1 + eightQ2 * q2q2 +
                    fourQ2 * az;
        double s3 = 4.0 * q1q1 * q3 - twoQ1 * ax + 4.0 * q2q2 * q3 -
                    twoQ2 * ay;

        const double gradientNormSquared = s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3;
        if (gradientNormSquared > kSmallNumber) {
            const double recipGradientNorm = inverseSqrt(gradientNormSquared);
            s0 *= recipGradientNorm;
            s1 *= recipGradientNorm;
            s2 *= recipGradientNorm;
            s3 *= recipGradientNorm;

            qDot0 -= beta_ * s0;
            qDot1 -= beta_ * s1;
            qDot2 -= beta_ * s2;
            qDot3 -= beta_ * s3;
        }
    }

    q0 += qDot0 * dtSeconds;
    q1 += qDot1 * dtSeconds;
    q2 += qDot2 * dtSeconds;
    q3 += qDot3 * dtSeconds;

    const double qNormSquared = q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3;
    if (qNormSquared <= kSmallNumber) {
        reset();
        return;
    }

    const double recipNorm = inverseSqrt(qNormSquared);
    q_.q0 = q0 * recipNorm;
    q_.q1 = q1 * recipNorm;
    q_.q2 = q2 * recipNorm;
    q_.q3 = q3 * recipNorm;
}

Quaternion MadgwickAHRS::quaternion() const {
    return q_;
}

EulerAngles MadgwickAHRS::eulerDegrees() const {
    const double sinRollCosPitch = 2.0 * (q_.q0 * q_.q1 + q_.q2 * q_.q3);
    const double cosRollCosPitch = 1.0 - 2.0 * (q_.q1 * q_.q1 + q_.q2 * q_.q2);
    const double roll = std::atan2(sinRollCosPitch, cosRollCosPitch);

    const double sinPitch = 2.0 * (q_.q0 * q_.q2 - q_.q3 * q_.q1);
    const double pitch = std::asin(std::clamp(sinPitch, -1.0, 1.0));

    const double sinYawCosPitch = 2.0 * (q_.q0 * q_.q3 + q_.q1 * q_.q2);
    const double cosYawCosPitch = 1.0 - 2.0 * (q_.q2 * q_.q2 + q_.q3 * q_.q3);
    const double yaw = std::atan2(sinYawCosPitch, cosYawCosPitch);

    return EulerAngles{
        radiansToDegrees(roll),
        radiansToDegrees(pitch),
        radiansToDegrees(yaw),
    };
}

double MadgwickAHRS::inverseSqrt(double value) {
    return 1.0 / std::sqrt(value);
}

double MadgwickAHRS::radiansToDegrees(double radians) {
    return radians * 180.0 / kPi;
}

}  // namespace imu
