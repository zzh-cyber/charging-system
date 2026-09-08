#include "runtimebootstrap.h"

#include <QByteArray>
#include <QDebug>
#include <QProcess>
#include <QStringList>

namespace
{

struct ProcessResult
{
    bool succeeded = false;
    QByteArray standardOutput;
};

ProcessResult runProcess(
    const QString &program,
    const QStringList &arguments,
    int timeoutMilliseconds = 3000)
{
    QProcess process;
    process.start(program, arguments);

    if (!process.waitForStarted(timeoutMilliseconds)) {
        return {};
    }

    if (!process.waitForFinished(timeoutMilliseconds)) {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }

    return {
        process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0,
        process.readAllStandardOutput().trimmed()
    };
}

} // namespace

namespace RuntimeBootstrap
{

void configureIbusEnvironment()
{
    qputenv("GTK_IM_MODULE", "ibus");
    qputenv("QT_IM_MODULE", "ibus");
    qputenv("XMODIFIERS", "@im=ibus");
}

void ensureIbusLibpinyin()
{
    ProcessResult engine =
        runProcess(
            QStringLiteral("ibus"),
            {QStringLiteral("engine")});

    if (!engine.succeeded) {
        const ProcessResult daemon =
            runProcess(
                QStringLiteral("ibus-daemon"),
                {QStringLiteral("-drx")});

        if (!daemon.succeeded) {
            qWarning() << "IBus could not be started; Chinese input may be unavailable.";
            return;
        }

        engine =
            runProcess(
                QStringLiteral("ibus"),
                {QStringLiteral("engine")});

        if (!engine.succeeded) {
            qWarning() << "IBus is not available; Chinese input may be unavailable.";
            return;
        }
    }

    if (engine.standardOutput == QByteArrayLiteral("libpinyin")) {
        return;
    }

    const ProcessResult switchEngine =
        runProcess(
            QStringLiteral("ibus"),
            {
                QStringLiteral("engine"),
                QStringLiteral("libpinyin")
            });

    if (!switchEngine.succeeded) {
        qWarning() << "IBus libpinyin could not be selected; Chinese input may be unavailable.";
    }
}

} // namespace RuntimeBootstrap
