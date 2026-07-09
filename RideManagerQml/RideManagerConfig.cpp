#include "RideManagerConfig.h"

#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <QtGlobal>

namespace {

QString valueFromEnvironment(const char *name, const QString &fallback)
{
    const QString value = qEnvironmentVariable(name).trimmed();
    return value.isEmpty() ? fallback : value;
}

QString connectionStringFromToml(const QString &text)
{
    static const QRegularExpression databaseSection(
        QStringLiteral(R"((?ms)^\s*\[database\]\s*$([\s\S]*?)(?=^\s*\[|\z))"));
    static const QRegularExpression connectionString(
        QStringLiteral(R"toml(^\s*connection_string\s*=\s*"([^"]*)"\s*$)toml"),
        QRegularExpression::MultilineOption);

    const QRegularExpressionMatch sectionMatch = databaseSection.match(text);
    if (!sectionMatch.hasMatch()) {
        return {};
    }

    const QRegularExpressionMatch valueMatch = connectionString.match(sectionMatch.captured(1));
    return valueMatch.hasMatch() ? valueMatch.captured(1).trimmed() : QString();
}

void applyConnectionPart(DatabaseConfig *config, const QString &key, const QString &value)
{
    const QString normalizedKey = key.trimmed().toLower();
    const QString normalizedValue = value.trimmed();

    if (normalizedKey == QStringLiteral("host") || normalizedKey == QStringLiteral("server")) {
        config->hostName = normalizedValue;
    } else if (normalizedKey == QStringLiteral("port")) {
        bool ok = false;
        const int port = normalizedValue.toInt(&ok);
        if (ok && port > 0 && port <= 65535) {
            config->port = port;
        }
    } else if (normalizedKey == QStringLiteral("database")) {
        config->databaseName = normalizedValue;
    } else if (normalizedKey == QStringLiteral("username")
               || normalizedKey == QStringLiteral("user id")
               || normalizedKey == QStringLiteral("user")) {
        config->userName = normalizedValue;
    } else if (normalizedKey == QStringLiteral("password")) {
        config->password = normalizedValue;
    }
}

DatabaseConfig parseConnectionString(const QString &connectionString)
{
    DatabaseConfig config;
    config.driverName = QStringLiteral("QPSQL");
    config.connectOptions = QStringLiteral("connect_timeout=3");

    const QUrl url(connectionString);
    if (url.isValid()
        && (url.scheme() == QStringLiteral("postgresql") || url.scheme() == QStringLiteral("postgres"))) {
        config.hostName = url.host().isEmpty() ? config.hostName : url.host();
        config.port = url.port(config.port);
        config.userName = url.userName(QUrl::FullyDecoded).isEmpty()
                              ? config.userName
                              : url.userName(QUrl::FullyDecoded);
        config.password = url.password(QUrl::FullyDecoded).isEmpty()
                              ? config.password
                              : url.password(QUrl::FullyDecoded);

        QString databaseName = url.path(QUrl::FullyDecoded);
        if (databaseName.startsWith(QLatin1Char('/'))) {
            databaseName.remove(0, 1);
        }
        if (!databaseName.isEmpty()) {
            config.databaseName = databaseName;
        }
    } else {
        const QStringList parts = connectionString.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            const qsizetype separator = part.indexOf(QLatin1Char('='));
            if (separator <= 0) {
                continue;
            }
            applyConnectionPart(&config, part.left(separator), part.mid(separator + 1));
        }
    }

    config.hostName = valueFromEnvironment("RIDEMANAGER_DB_HOST", config.hostName);
    config.databaseName = valueFromEnvironment("RIDEMANAGER_DB_NAME", config.databaseName);
    config.userName = valueFromEnvironment("RIDEMANAGER_DB_USER", config.userName);
    config.password = valueFromEnvironment("RIDEMANAGER_DB_PASSWORD", config.password);

    bool portOk = false;
    const int environmentPort = qEnvironmentVariableIntValue("RIDEMANAGER_DB_PORT", &portOk);
    if (portOk && environmentPort > 0 && environmentPort <= 65535) {
        config.port = environmentPort;
    }

    return config;
}

} // namespace

RideManagerConfigResult RideManagerConfig::load()
{
    RideManagerConfigResult result;

    const QString environmentConnectionString =
        valueFromEnvironment("RIDEMANAGER_DATABASE_URL",
                             valueFromEnvironment("RIDEMANAGER_CONNECTION_STRING", QString()));
    if (!environmentConnectionString.isEmpty()) {
        result.configPath = QStringLiteral("environment");
        result.database = parseConnectionString(environmentConnectionString);
        result.endpoint = QStringLiteral("%1:%2/%3")
                              .arg(result.database.hostName)
                              .arg(result.database.port)
                              .arg(result.database.databaseName);
        result.ok = true;
        return result;
    }

    result.configPath = valueFromEnvironment("RIDEMANAGER_CONFIG_PATH", defaultConfigPath());

    QFile file(result.configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.configPath = QStringLiteral("built-in default");
        result.database = parseConnectionString(
            QStringLiteral("postgresql://ridemanager:ridemanager@127.0.0.1:5432/ridemanager"));
        result.endpoint = QStringLiteral("%1:%2/%3")
                              .arg(result.database.hostName)
                              .arg(result.database.port)
                              .arg(result.database.databaseName);
        result.ok = true;
        return result;
    }

    QTextStream stream(&file);
    const QString connectionString = connectionStringFromToml(stream.readAll());
    if (connectionString.isEmpty()) {
        result.errorMessage =
            QStringLiteral("RideManager 配置缺少 [database].connection_string");
        return result;
    }

    result.database = parseConnectionString(connectionString);
    result.endpoint = QStringLiteral("%1:%2/%3")
                          .arg(result.database.hostName)
                          .arg(result.database.port)
                          .arg(result.database.databaseName);
    result.ok = true;
    return result;
}

QString RideManagerConfig::defaultConfigPath()
{
    const QString localConfig = QStringLiteral("config.toml");
    if (QFile::exists(localConfig)) {
        return localConfig;
    }

    const QString applicationConfig =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config.toml"));
    if (QFile::exists(applicationConfig)) {
        return applicationConfig;
    }

    return QStringLiteral("config.toml");
}
