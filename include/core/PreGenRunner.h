#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include "WorldGenTypes.h"
#include "ServerCoreInstaller.h"

// 負責：
// 1. 寫入 eula.txt=true
// 2. 依 LaunchSpec（由 ServerCoreInstaller 解析出來）啟動對應的程式/腳本
// 3. 即時把 stdout/stderr 轉發成 consoleLine signal（供 UI 顯示）
// 4. 偵測到 "Done (" 代表世界已生成完成，自動送出 "stop" 指令關閉伺服器
// 5. 逾時保護：超過 pregenTimeoutSec 仍未完成則強制終止
class PreGenRunner : public QObject
{
    Q_OBJECT
public:
    explicit PreGenRunner(QObject *parent = nullptr);

    void start(const WorldGenConfig &config, const LaunchSpec &spec);

    void abort();

signals:
    void consoleLine(const QString &line);
    void pregenFinished(bool success, const QString &message);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void writeEula(const QString &workingDir);

    QProcess *m_process = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    bool m_stopRequested = false;
};
