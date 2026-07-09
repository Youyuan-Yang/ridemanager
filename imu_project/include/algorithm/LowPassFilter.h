#pragma once

#include <optional>
#include <stdexcept>

#include "common/IMUData.h"

namespace imu {

class LowPassFilter {
public:
    explicit LowPassFilter(double alpha) {
        setAlpha(alpha);
    }

    void setAlpha(double alpha) {
        if (alpha <= 0.0 || alpha > 1.0) {
            throw std::invalid_argument("lowpass alpha must be in (0, 1]");
        }
        alpha_ = alpha;
    }

    double update(double input) {
        if (!lastOutput_.has_value()) {
            lastOutput_ = input;
            return input;
        }

        const double output = alpha_ * input + (1.0 - alpha_) * lastOutput_.value();
        lastOutput_ = output;
        return output;
    }

    void reset() {
        lastOutput_.reset();
    }

private:
    double alpha_{0.1};
    std::optional<double> lastOutput_;
};

class VectorLowPassFilter {
public:
    explicit VectorLowPassFilter(double alpha) : x_(alpha), y_(alpha), z_(alpha) {}

    void setAlpha(double alpha) {
        x_.setAlpha(alpha);
        y_.setAlpha(alpha);
        z_.setAlpha(alpha);
    }

    Vector3 update(const Vector3& input) {
        return Vector3{x_.update(input.x), y_.update(input.y), z_.update(input.z)};
    }

    void reset() {
        x_.reset();
        y_.reset();
        z_.reset();
    }

private:
    LowPassFilter x_;
    LowPassFilter y_;
    LowPassFilter z_;
};

}  // namespace imu
