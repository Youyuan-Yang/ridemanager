#include "config/ConfigLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace imu {

namespace {

std::string trim(const std::string& text) {
    auto begin = text.begin();
    while (begin != text.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }

    auto end = text.end();
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }

    return std::string(begin, end);
}

std::string unquote(const std::string& text) {
    if (text.size() >= 2) {
        const char first = text.front();
        const char last = text.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return text.substr(1, text.size() - 2);
        }
    }
    return text;
}

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::string part;
    std::istringstream stream(text);

    while (std::getline(stream, part, delimiter)) {
        parts.push_back(part);
    }

    return parts;
}

std::string normalizeConnectionString(const std::string& value) {
    if (value.find(';') == std::string::npos) {
        return value;
    }

    std::string host;
    std::string port;
    std::string database;
    std::string username;
    std::string password;

    for (const auto& part : split(value, ';')) {
        const std::size_t equals = part.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = toLower(trim(part.substr(0, equals)));
        const std::string fieldValue = trim(part.substr(equals + 1));

        if (key == "host" || key == "server") {
            host = fieldValue;
        } else if (key == "port") {
            port = fieldValue;
        } else if (key == "database" || key == "dbname") {
            database = fieldValue;
        } else if (key == "username" || key == "user" || key == "userid" || key == "user id") {
            username = fieldValue;
        } else if (key == "password") {
            password = fieldValue;
        }
    }

    std::ostringstream conninfo;
    if (!host.empty()) {
        conninfo << "host=" << host << ' ';
    }
    if (!port.empty()) {
        conninfo << "port=" << port << ' ';
    }
    if (!database.empty()) {
        conninfo << "dbname=" << database << ' ';
    }
    if (!username.empty()) {
        conninfo << "user=" << username << ' ';
    }
    if (!password.empty()) {
        conninfo << "password=" << password << ' ';
    }
    conninfo << "connect_timeout=3";
    return trim(conninfo.str());
}

int parseInt(const std::string& text) {
    std::size_t parsed = 0;
    const int value = std::stoi(text, &parsed, 0);
    if (parsed != text.size()) {
        throw std::invalid_argument("invalid integer value: " + text);
    }
    return value;
}

double parseDouble(const std::string& text) {
    std::size_t parsed = 0;
    const double value = std::stod(text, &parsed);
    if (parsed != text.size()) {
        throw std::invalid_argument("invalid double value: " + text);
    }
    return value;
}

bool parseBool(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (text == "true" || text == "1" || text == "yes") {
        return true;
    }
    if (text == "false" || text == "0" || text == "no") {
        return false;
    }

    throw std::invalid_argument("invalid bool value: " + text);
}

}  // namespace

AppConfig ConfigLoader::load(const std::string& path) {
    AppConfig config;

    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open config file: " + path);
    }

    std::string line;
    int lineNumber = 0;
    std::string section;
    while (std::getline(input, line)) {
        ++lineNumber;

        const std::string stripped = trim(line);
        if (stripped.empty() || stripped.front() == '#') {
            continue;
        }

        if (stripped.front() == '[' && stripped.back() == ']') {
            std::size_t begin = 1;
            std::size_t end = stripped.size() - 1;
            if (stripped.size() >= 4 && stripped[1] == '[' && stripped[stripped.size() - 2] == ']') {
                begin = 2;
                end = stripped.size() - 2;
            }
            section = trim(stripped.substr(begin, end - begin));
            continue;
        }

        const std::size_t colon = stripped.find(':');
        const std::size_t equals = stripped.find('=');
        const bool useColon = colon != std::string::npos &&
                              (equals == std::string::npos || colon < equals);
        const std::size_t delimiter = useColon ? colon : equals;

        if (delimiter == std::string::npos) {
            throw std::runtime_error("invalid config line " + std::to_string(lineNumber));
        }

        const std::string key = trim(stripped.substr(0, delimiter));
        const std::string value = unquote(trim(stripped.substr(delimiter + 1)));
        applyValue(config, section, key, value);
    }

    return config;
}

void ConfigLoader::applyValue(
    AppConfig& config,
    const std::string& section,
    const std::string& key,
    const std::string& value) {
    const bool rootSection = section.empty();
    const bool imuSection = section == "imu" || section == "sensors.imu";
    const bool databaseSection = section == "database";

    if (key == "i2c_device") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.i2cDevice = value;
    } else if (key == "address") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.address = parseInt(value);
    } else if (key == "sample_rate") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.sampleRateHz = parseDouble(value);
    } else if (key == "database_rate") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.databaseRateHz = parseDouble(value);
    } else if (key == "madgwick_beta") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.madgwickBeta = parseDouble(value);
    } else if (key == "lowpass_alpha") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.lowpassAlpha = parseDouble(value);
    } else if (key == "calibration_enabled" || key == "gyro_calibration_enabled") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.calibrationEnabled = parseBool(value);
    } else if (key == "calibration_samples" || key == "gyro_calibration_samples") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.calibrationSamples = parseInt(value);
    } else if (key == "calibration_max_gyro_stddev") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.calibrationMaxGyroStddevDps = parseDouble(value);
    } else if (key == "calibration_max_accel_stddev") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.calibrationMaxAccelStddevG = parseDouble(value);
    } else if (key == "mount_x_sign" || key == "axis_x_sign") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.mountXSign = parseInt(value);
    } else if (key == "mount_y_sign" || key == "axis_y_sign") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.mountYSign = parseInt(value);
    } else if (key == "mount_z_sign" || key == "axis_z_sign") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.mountZSign = parseInt(value);
    } else if (key == "accel_scale_correction") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.accelScaleCorrection = parseDouble(value);
    } else if (key == "enable_database") {
        if (!rootSection && !databaseSection) {
            return;
        }
        config.enableDatabase = parseBool(value);
    } else if (key == "database_conninfo" || key == "connection_string") {
        if (!rootSection && !databaseSection) {
            return;
        }
        config.databaseConninfo = normalizeConnectionString(value);
    } else if (key == "device_id") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.deviceId = value;
    } else if (key == "safety_decision_id") {
        if (!rootSection && !imuSection) {
            return;
        }
        if (value.empty()) {
            config.safetyDecisionId.reset();
        } else {
            config.safetyDecisionId = value;
        }
    } else if (key == "sensor_name") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.sensorName = value;
    } else if (key == "queue_capacity") {
        if (!rootSection && !imuSection) {
            return;
        }
        config.queueCapacity = static_cast<std::size_t>(parseInt(value));
    }
}

}  // namespace imu
