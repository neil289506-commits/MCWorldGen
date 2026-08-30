#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QFile>
#include <functional>
#include "ModloaderInfo.h"
#include "WorldGenTypes.h"

// 執行完安裝後，PreGenRunner 需要知道「該怎麼啟動這個伺服器」：
// 有的載入器（Vanilla/Fabric/Quilt/Paper）直接 java -jar 某個 jar 即可，
// 有的載入器（Forge/NeoForge，1.18.1 之後）改成執行 installer 產生的
// run.bat / run.sh 腳本（內部才是真正組好的 java 指令與 classpath）。
struct LaunchSpec {
    bool valid = false;
    QString program;       // 可執行檔名稱，例如 "java" 或 "run.bat" / "bash"
    QStringList arguments;
    QString workingDir;
};

// 改寫自參考專案（MCServer-Manager-17）的 src/installer/InstallerWorker.cpp
// 與 src/installer/PaperPurpurDownloader.cpp：
// - 只保留 Server 端安裝（worldgen 不需要 Client 端）
// - 拿掉 OptiFine（worldgen 用不到）
// - 安裝完成後額外解析出 LaunchSpec，交給 PreGenRunner 直接執行
class ServerCoreInstaller : public QObject
{
    Q_OBJECT
public:
    explicit ServerCoreInstaller(QObject *parent = nullptr);
    ~ServerCoreInstaller();

    // installDir 建議直接傳入 %TEMP% 下的預生成資料夾（跟其他步驟共用同一個目錄）
    void install(const WorldGenConfig &config, const QString &installDir);
    void cancel();

signals:
    void progressMessage(const QString &msg);
    void progressPercent(int percent); // -1 = 不確定進度
    void installFinished(bool success, const QString &message, const LaunchSpec &spec);

private slots:
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();
    void startDownload(const QUrl &url, const QString &destPath, std::function<void()> onSuccess);
    void runInstallerJar(const QString &jarPath, const QStringList &extraArgs,
                          std::function<void(bool)> onFinished);
    void ensureLauncherProfile(const QString &dir);
    void hideProcessWindow(QProcess *p);

    void installVanilla();
    void installFabric();
    void installQuilt();
    void installForge();
    void installNeoForge();
    void installPaper();

    // 判斷 Forge/NeoForge 是否為「1.18.1+ 新式安裝」（產生 run.bat/run.sh 而非直接 universal jar）
    static bool mcAtLeast(const QString &ver, int minor, int patch = 0);
    // 依載入器安裝完成後的資料夾內容，組出可直接執行的 LaunchSpec
    LaunchSpec resolveLaunchSpec(bool forgeNewStyle) const;

    WorldGenConfig m_config;
    QString m_installDir;

    QNetworkAccessManager *m_nam     = nullptr;
    QNetworkReply         *m_reply   = nullptr;
    QFile                 *m_outFile = nullptr;
    QProcess              *m_process = nullptr;
    bool                   m_cancelled = false;

    std::function<void()>     m_downloadCallback;
    std::function<void(bool)> m_processCallback;
};
