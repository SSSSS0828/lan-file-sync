#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include "ui/MainWindow.h"
#include "core/TransferManager.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LanSwiftTransfer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LanSwift");

    QCommandLineParser parser;
    parser.setApplicationDescription("High-performance LAN file transfer tool");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption modeOpt(
        "mode", "Run mode: gui (default), server, client", "mode", "gui");
    QCommandLineOption hostOpt(
        "host", "Remote host (client mode)", "host", "127.0.0.1");
    QCommandLineOption portOpt(
        "port", "Port number", "port", "9527");
    QCommandLineOption fileOpt(
        "file", "File to send (client mode)", "file");
    QCommandLineOption saveDirOpt(
        "save-dir", "Directory to save received files (server mode)",
        "dir", QDir::homePath() + "/Downloads/LanSwift");

    parser.addOption(modeOpt);
    parser.addOption(hostOpt);
    parser.addOption(portOpt);
    parser.addOption(fileOpt);
    parser.addOption(saveDirOpt);
    parser.process(app);

    QString mode = parser.value(modeOpt);

    if (mode == "server") {
        // Headless server mode
        TransferManager mgr;
        quint16 port = static_cast<quint16>(parser.value(portOpt).toUInt());
        QString dir  = parser.value(saveDirOpt);
        QDir().mkpath(dir);

        QObject::connect(mgr.receiver(), &RecvWorker::statusMessage,
                         [](const QString& msg) {
            qInfo() << "[STATUS]" << msg;
        });
        QObject::connect(mgr.receiver(), &RecvWorker::progressUpdated,
                         [](qint64 t, qint64 total) {
            if (total > 0)
                qInfo().nospace()
                    << "[PROGRESS] "
                    << (t * 100 / total) << "% ("
                    << t << "/" << total << " bytes)";
        });
        QObject::connect(mgr.receiver(), &RecvWorker::transferFinished,
                         [&app](bool ok, const QString& msg) {
            qInfo() << (ok ? "[DONE]" : "[FAIL]") << msg;
            // Stay alive for next transfer
        });

        mgr.startServer(port, dir);
        qInfo() << "Server started on port" << port
                << "- saving to" << dir;
        return app.exec();
    }

    if (mode == "client") {
        if (!parser.isSet(fileOpt)) {
            qCritical() << "Client mode requires --file <path>";
            return 1;
        }
        TransferManager mgr;
        QString host = parser.value(hostOpt);
        quint16 port = static_cast<quint16>(parser.value(portOpt).toUInt());
        QString file = parser.value(fileOpt);

        QObject::connect(mgr.sender(), &SendWorker::statusMessage,
                         [](const QString& msg) {
            qInfo() << "[STATUS]" << msg;
        });
        QObject::connect(mgr.sender(), &SendWorker::progressUpdated,
                         [](qint64 t, qint64 total) {
            if (total > 0)
                qInfo().nospace()
                    << "[PROGRESS] "
                    << (t * 100 / total) << "%";
        });
        QObject::connect(mgr.sender(), &SendWorker::speedUpdated,
                         [](double bps) {
            qInfo().nospace()
                << "[SPEED] "
                << QString::number(bps / 1024.0 / 1024.0, 'f', 2)
                << " MB/s";
        });
        QObject::connect(mgr.sender(), &SendWorker::transferFinished,
                         [&app](bool ok, const QString& msg) {
            qInfo() << (ok ? "[DONE]" : "[FAIL]") << msg;
            app.quit();
        });

        mgr.sendFile(file, host, port);
        return app.exec();
    }

    // GUI mode (default)
    MainWindow win;
    win.show();
    return app.exec();
}
