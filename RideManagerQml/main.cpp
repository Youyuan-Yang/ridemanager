#include <QGuiApplication>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "AppController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("RideManager"));
    QGuiApplication::setApplicationName(QStringLiteral("RideManagerQml"));
    app.setFont(QFont(QStringLiteral("PingFang SC")));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    AppController controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/RideManager/Main.qml")));
    return app.exec();
}
