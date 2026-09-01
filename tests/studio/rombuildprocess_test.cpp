#include "studio/rombuildprocess.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include <iostream>

using namespace Studio;

static bool runAndWait(RomBuildProcess &process, const RomBuildRequest &request,
                       RomBuildResult *result, QString *startError = nullptr)
{
    QEventLoop loop;
    QObject::connect(&process, &RomBuildProcess::finished, &loop,
                     [&](const RomBuildResult &finished) {
                         *result = finished;
                         loop.quit();
                     });
    if (!process.start(request, startError))
        return false;
    loop.exec();
    return true;
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QString cwd = QDir::tempPath();
    RomBuildProcess process;
    QStringList output;
    QObject::connect(&process, &RomBuildProcess::outputReceived,
                     [&](const QString &text, bool standardError) {
                         output.append((standardError ? QStringLiteral("E:") : QStringLiteral("O:")) + text);
                     });

    RomBuildRequest success{QStringLiteral("/bin/sh"),
                            {QStringLiteral("-c"), QStringLiteral("printf 'build ok'; printf 'warning' >&2")},
                            cwd, 5000, RomBuildOperation::Build};
    RomBuildResult result;
    if (!runAndWait(process, success, &result) || !result.success
        || !result.standardOutput.contains(QStringLiteral("build ok"))
        || !result.standardError.contains(QStringLiteral("warning"))
        || output.size() < 2) {
        std::cerr << "Successful streaming command failed.\n";
        return 1;
    }

    RomBuildRequest failure{QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("exit 7")},
                            cwd, 5000, RomBuildOperation::Test};
    if (!runAndWait(process, failure, &result) || result.success || result.exitCode != 7
        || !result.error.contains(QStringLiteral("exit code 7"))) {
        std::cerr << "Nonzero command reporting failed.\n";
        return 1;
    }

    RomBuildRequest timeout{QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("sleep 2")},
                            cwd, 50, RomBuildOperation::Build};
    if (!runAndWait(process, timeout, &result) || result.success || !result.timedOut) {
        std::cerr << "Timeout handling failed.\n";
        return 1;
    }

    RomBuildRequest cancel{QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("sleep 2")},
                           cwd, 5000, RomBuildOperation::Test};
    QTimer::singleShot(50, &process, &RomBuildProcess::cancel);
    if (!runAndWait(process, cancel, &result) || result.success || !result.canceled) {
        std::cerr << "Cancellation handling failed.\n";
        return 1;
    }

    QString error;
    RomBuildRequest invalid{QStringLiteral("/bin/sh"), {}, QStringLiteral("/not/a/real/directory"), 5000};
    if (process.start(invalid, &error) || error.isEmpty()) {
        std::cerr << "Invalid working directory should be rejected.\n";
        return 1;
    }
    const QString specialText = QStringLiteral("  path # value / العربية  ");
    bool decodeOk = false;
    if (RomBuildProcess::decodeText(RomBuildProcess::encodeText(specialText), &decodeOk) != specialText
        || !decodeOk) {
        std::cerr << "Encoded text round-trip failed.\n";
        return 1;
    }
    const QStringList specialArguments{
        QStringLiteral("--define=#value"), QStringLiteral("  spaced  "),
        QString(), QStringLiteral("unit") + QChar(0x1f) + QStringLiteral("separator")
    };
    if (RomBuildProcess::decodeArguments(RomBuildProcess::encodeArguments(specialArguments), &decodeOk)
            != specialArguments || !decodeOk) {
        std::cerr << "Encoded arguments round-trip failed.\n";
        return 1;
    }
    std::cout << "All ROM build process tests passed.\n";
    return 0;
}