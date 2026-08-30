#include "core/PreGenRunner.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#ifdef Q_OS_WIN
#  include <windows.h>
#endif

PreGenRunner::PreGenRunner(QObject *parent) : QObject(parent)
{
}

void PreGenRunner::writeEula(const QString &workingDir)
{
    QFile file(workingDir + "/eula.txt");
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream(&file) << "eula=true\n";
        file.close();
    }
}

void PreGenRunner::start(const WorldGenConfig &config, const LaunchSpec &spec)
{
    if (!spec.valid) {
        emit consoleLine(QStringLiteral(">>> 找不到可執行的伺服器核心，無法啟動預生成"));
        emit pregenFinished(false, QStringLiteral("LaunchSpec 無效"));
        return;
    }

    m_stopRequested = false;
    writeEula(spec.workingDir);

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(spec.workingDir);

    // java（以及 Forge/NeoForge 的 run.bat/cmd）預設會另外彈出一個主控台視窗，
    // 疊在主視窗後面看起來像沒關掉的終端機。這裡用 CREATE_NO_WINDOW 讓它完全不開視窗，
    // 輸出仍然透過 stdout/stderr 導管正常轉發到我們自己的 console。
#ifdef Q_OS_WIN
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW
        args->startupInfo->dwFlags |= 0x01; // STARTF_USESHOWWINDOW
        args->startupInfo->wShowWindow = 0;  // SW_HIDE
    });
#endif

    connect(m_process, &QProcess::readyReadStandardOutput, this, &PreGenRunner::onReadyReadStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &PreGenRunner::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PreGenRunner::onProcessFinished);

    emit consoleLine(QStringLiteral(">>> 啟動預生成程序：%1 %2")
                      .arg(spec.program, spec.arguments.join(' ')));

    m_process->start(spec.program, spec.arguments);

    // 逾時保護：避免部分模組導致伺服器卡死不結束
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        emit consoleLine(QStringLiteral(">>> 預生成逾時，強制終止程序"));
        abort();
    });
    m_timeoutTimer->start(config.pregenTimeoutSec * 1000);
}

void PreGenRunner::onReadyReadStdout()
{
    const QString chunk = QString::fromUtf8(m_process->readAllStandardOutput());
    for (const QString &line : chunk.split('\n', Qt::SkipEmptyParts)) {
        emit consoleLine(line);

        // 世界生成完成的判斷式：Vanilla/Fabric/Forge 等日誌皆會輸出 "Done (" 開頭的行
        if (!m_stopRequested && line.contains("Done (")) {
            m_stopRequested = true;
            emit consoleLine(QStringLiteral(">>> 偵測到世界生成完成，送出 stop 指令"));
            m_process->write("stop\n");
        }
    }
}

void PreGenRunner::onReadyReadStderr()
{
    const QString chunk = QString::fromUtf8(m_process->readAllStandardError());
    for (const QString &line : chunk.split('\n', Qt::SkipEmptyParts))
        emit consoleLine(QStringLiteral("[stderr] %1").arg(line));
}

void PreGenRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        m_timeoutTimer->deleteLater();
        m_timeoutTimer = nullptr;
    }

    const bool success = (status == QProcess::NormalExit && (exitCode == 0 || m_stopRequested));
    emit pregenFinished(success,
        success ? QStringLiteral("預生成完成") : QStringLiteral("伺服器異常結束（exit code %1）").arg(exitCode));
}

void PreGenRunner::abort()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->write("stop\n");
        if (!m_process->waitForFinished(5000))
            m_process->kill();
    }
}
