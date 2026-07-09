#ifndef RIDEMANAGERCONFIG_H
#define RIDEMANAGERCONFIG_H

#include <QString>

#include "DatabaseManager.h"

struct RideManagerConfigResult
{
    bool ok = false;
    QString errorMessage;
    QString configPath;
    QString endpoint;
    DatabaseConfig database;
};

class RideManagerConfig
{
public:
    static RideManagerConfigResult load();

private:
    static QString defaultConfigPath();
};

#endif // RIDEMANAGERCONFIG_H
