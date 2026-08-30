#pragma once
#include <QObject>
#include <QProcess>
#include <QDir>
#include <QStringList>
#include <QNetworkAccessManager>
#include "ModloaderInfo.h"

// 改寫自參考專案（MCServer-Manager-17）的 src/installer/VersionFetcher.h/.cpp。
//
// 原始設計：用一支自動產生的 Python 腳本（requestVer.py）一次把 Vanilla /
// Fabric / Quilt / Forge / NeoForge 的版本清單檔案都下載到 <exe目錄>/version/，
// 再由 C++ 端逐一解析各檔案格式（JSON / Maven metadata XML）。
// 好處：一次 QProcess 呼叫換來 5 種來源的資料，且 Python + requests 對某些
// 企業網路環境的相容性比 Qt 的 QNetworkAccessManager 更好處理 TLS/Proxy。
//
// 這裡保留完全相同的抓取/解析邏輯，只移除 worldgen 用不到的 OptiFine 部分，
// 並另外加上 fetchPaperVersions()：Paper 官方 API 本身就很輕量，直接用
// QNetworkAccessManager 查詢即可，不需要透過 Python 腳本。
class VersionFetcher : public QObject
{
    Q_OBJECT
public:
    explicit VersionFetcher(QObject *parent = nullptr);

    // 觸發 Python 腳本一次抓齊 Vanilla/Fabric/Quilt/Forge/NeoForge 的版本資料，
    // 完成後自動呼叫 parseLocal() 並發出對應的 *versionsReady 訊號
    void fetchAll();

    // 直接解析已經下載好的版本資料夾（例如上次執行留下的快取，不想重新連網時使用）
    void parseLocal(const QString &versionDir);

    bool isFetching() const { return m_fetching; }

    // Paper 版本清單走獨立的輕量 API 查詢，不需要等 fetchAll() 的 Python 流程
    void fetchPaperVersions();

signals:
    void fetchProgress(const QString &message);
    void fetchFinished();
    void fetchError(const QString &error);

    // 載入器版本清單：
    // - Vanilla：mcVersion 有值、loaderVersion 空 → 直接當「遊戲版本」清單使用
    // - Fabric/Quilt：loaderVersion 有值、mcVersion 空 → 獨立於遊戲版本的載入器版本清單
    // - Forge/NeoForge：mcVersion + loaderVersion 成對 → UI 需先依 mcVersion 分組
    void versionsReady(ModloaderType type, const QList<ModloaderVersion> &versions);

    // Fabric/Quilt 專用：該載入器支援的遊戲版本清單
    void gameVersionsReady(ModloaderType type, const QStringList &mcVersions);

private slots:
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void generatePythonScript(const QString &scriptPath);
    void parseVanilla(const QString &dir);
    void parseFabric(const QString &dir);
    void parseQuilt(const QString &dir);
    void parseForge(const QString &dir);
    void parseNeoForge(const QString &dir);

    QProcess *m_process = nullptr;
    bool      m_fetching = false;
    QString   m_versionDir;

    QNetworkAccessManager *m_net;
};
