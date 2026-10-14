#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QElapsedTimer>
#include <QThread>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <KCModule>
#include <iostream>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusPendingReply>
#include <QDBusArgument>
#include <QJsonDocument>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (argc >= 2 && (QString::fromLocal8Bit(argv[1]) == QStringLiteral("--call")
        || QString::fromLocal8Bit(argv[1]) == QStringLiteral("--busy"))) {
        QDBusInterface iface(QStringLiteral("org.kde.KWin"), QStringLiteral("/com/xronlinux/BreezyDesktop"),
                             QStringLiteral("com.xronlinux.BreezyDesktop"), QDBusConnection::sessionBus());
        if (QString::fromLocal8Bit(argv[1]) == QStringLiteral("--busy")) {
            QDBusPendingReply<QVariantMap> first = iface.asyncCall(QStringLiteral("ReloadScene"));
            QDBusPendingReply<QVariantMap> second = iface.asyncCall(QStringLiteral("ReloadScene"));
            first.waitForFinished(); second.waitForFinished();
            if (first.isError() || second.isError()) return 9;
            std::cout << QJsonDocument::fromVariant(QVariantList{first.value(), second.value()}).toJson(QJsonDocument::Compact).constData();
            return 0;
        }
        if (argc < 3) return 2;
        const auto method = QString::fromLocal8Bit(argv[2]);
        QVariantList args;
        for (int i = 3; i < argc; ++i) {
            const auto argument = QString::fromLocal8Bit(argv[i]);
            args << (method == QStringLiteral("RemoveVirtualDisplay") ? QVariant(argument) : QVariant(argument.toInt()));
        }
        const auto message = iface.callWithArgumentList(QDBus::Block, method, args);
        if (message.type() == QDBusMessage::ErrorMessage) {
            std::cerr << message.errorMessage().toStdString(); return 8;
        }
        QVariant value;
        if (method == QStringLiteral("ListVirtualDisplays") || method == QStringLiteral("AddVirtualDisplay")
            || method == QStringLiteral("RemoveVirtualDisplay")) {
            QDBusReply<QVariantList> reply(message);
            if (!reply.isValid()) return 7;
            auto rows = reply.value();
            for (auto &row : rows) {
                if (row.metaType() == QMetaType::fromType<QDBusArgument>()) row = qdbus_cast<QVariantMap>(row.value<QDBusArgument>());
            }
            value = rows;
        } else {
            QDBusReply<QVariantMap> reply(message);
            if (!reply.isValid()) return 7;
            value = reply.value();
        }
        std::cout << QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact).constData();
        return 0;
    }
    const bool expectStale = argc == 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--expect-stale");
    if (argc != 2 && !expectStale) return 2;
    auto result = KPluginFactory::instantiatePlugin<KCModule>(KPluginMetaData(QString::fromLocal8Bit(argv[expectStale ? 2 : 1])), nullptr);
    if (!result.plugin) { std::cerr << result.errorText.toStdString(); return 3; }
    auto *button = result.plugin->widget()->findChild<QPushButton *>(QStringLiteral("buttonReloadScene"));
    auto *label = result.plugin->widget()->findChild<QLabel *>(QStringLiteral("labelReloadStatus"));
    if (!button || !label) { std::cerr << "RED: reload controls absent\n"; return 4; }
    QElapsedTimer timer;
    timer.start();
    while (!button->isEnabled() && timer.elapsed() < 5000) { app.processEvents(); QThread::msleep(10); }
    if (!button->isEnabled()) { std::cerr << label->text().toStdString(); return 5; }
    button->click();
    timer.restart();
    while (!button->isEnabled() && timer.elapsed() < 7000) { app.processEvents(); QThread::msleep(10); }
    const auto text = label->text();
    if (!button->isEnabled() || !text.contains(QStringLiteral("preserved"))) {
        std::cerr << "KCM reload failed: " << text.toStdString(); return 6;
    }
    if (expectStale && !text.contains(QStringLiteral("differs"))) {
        std::cerr << "KCM did not warn about stale C++ binary: " << text.toStdString(); return 10;
    }
    std::cout << "PASS real KCM button: " << text.toStdString() << '\n';
    delete result.plugin;
}
